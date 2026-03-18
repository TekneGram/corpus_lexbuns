#include "experiment_engine.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../analysis_layer/artifact_inspector.hpp"
#include "../analysis_layer/comparison_analyzer.hpp"
#include "../analysis_layer/extraction_diagnostics_builder.hpp"
#include "../analysis_layer/robust_set_builder.hpp"
#include "../analysis_layer/sampling_diagnostics_builder.hpp"
#include "../analysis_layer/stability_analyzer.hpp"
#include "../extraction_layer/conditional_extractor.hpp"
#include "../extraction_layer/unconditional_extractor.hpp"
#include "../sampling_layer/corpus_sampler.hpp"
#include "../sampling_layer/document_pool_builder.hpp"
#include "../sampling_layer/sample_condition_planner.hpp"
#include "../experiment_shared_layer/json_io.hpp"

namespace teknegram {

namespace {

std::string FormatUnsigned(std::uint32_t value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned int>(value));
    return buffer;
}

std::string JoinPathLocal(const std::string& left, const std::string& right) {
    if (left.empty()) {
        return right;
    }
    if (left[left.size() - 1] == '/') {
        return left + right;
    }
    return left + "/" + right;
}

const CompositionConfig* FindCompositionConfig(const ExperimentOptions& options,
                                               const std::string& label) {
    for (std::size_t i = 0; i < options.composition_configs.size(); ++i) {
        if (options.composition_configs[i].label == label) {
            return &options.composition_configs[i];
        }
    }
    return 0;
}

std::string ArtifactRootFor(const ExperimentOptions& options) {
    if (!options.artifact_input_dir.empty()) {
        return options.artifact_input_dir;
    }
    if (!options.requested_experiment_code.empty()) {
        return JoinPathLocal(
            JoinPathLocal(options.dataset_dir, "experiments"),
            options.requested_experiment_code);
    }
    return JoinPathLocal(options.dataset_dir, "experiments");
}

std::vector<ExperimentCondition> ConditionsForDesign(const std::vector<ExperimentCondition>& conditions,
                                                     const std::string& sampling_design_id) {
    std::vector<ExperimentCondition> filtered;
    for (std::size_t i = 0; i < conditions.size(); ++i) {
        if (conditions[i].sampling_design_id == sampling_design_id) {
            filtered.push_back(conditions[i]);
        }
    }
    return filtered;
}

std::string RobustSetArtifactName(const ExperimentCondition& condition) {
    return std::string("robust_set_") + condition.condition_id + ".json";
}

std::string StabilityArtifactName(const ExperimentCondition& condition) {
    return std::string("stability_") + condition.condition_id + ".json";
}

std::string AggregateArtifactName(const ExperimentCondition& condition) {
    return std::string("aggregate_") + condition.condition_id + ".bin";
}

std::string ExtractionDiagnosticsPath(const std::string& artifact_root,
                                      const ExperimentCondition& condition) {
    return JoinPathLocal(artifact_root, ExtractionDiagnosticsArtifactName(condition));
}

std::vector<std::string> ListDirectoryFiles(const std::string& dir_path) {
    std::vector<std::string> paths;
    DIR* dir = opendir(dir_path.c_str());
    if (!dir) {
        return paths;
    }
    for (;;) {
        dirent* entry = readdir(dir);
        if (!entry) {
            break;
        }
        const std::string name = entry->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        const std::string path = JoinPathLocal(dir_path, name);
        struct stat st;
        if (stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode)) {
            paths.push_back(path);
        }
    }
    closedir(dir);
    return paths;
}

bool IsJsonPath(const std::string& path) {
    return path.size() >= 5U && path.substr(path.size() - 5U) == ".json";
}

ExperimentOptions LoadOptionsFromRunManifest(const std::string& artifact_root) {
    const std::string manifest_path = JoinPathLocal(artifact_root, "run_manifest.json");
    std::ifstream input(manifest_path.c_str(), std::ios::in | std::ios::binary);
    if (!input) {
        throw std::runtime_error("Run manifest not found: " + manifest_path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(buffer.str());
    } catch (const std::exception&) {
        throw std::runtime_error("Failed to parse run manifest: " + manifest_path);
    }

    if (!payload.is_object() || !payload.contains("options") || !payload["options"].is_object()) {
        throw std::runtime_error("Run manifest missing options payload: " + manifest_path);
    }
    return ExperimentOptions::FromJson(payload["options"]);
}

} // namespace

ExperimentEngine::ExperimentEngine() {}

void ExperimentEngine::validate_options(const ExperimentOptions& options) const {
    options.Validate();
}

CorpusDataset ExperimentEngine::open_dataset(const ExperimentOptions& options) const {
    CorpusDataset dataset;
    dataset.open(options.dataset_dir, options.bundle_size);
    return dataset;
}

std::vector<SamplingDesign> ExperimentEngine::build_sampling_designs(const ExperimentOptions& options) const {
    SampleConditionPlanner planner;
    return planner.build_sampling_designs(options);
}

ExperimentCondition ExperimentEngine::build_condition(const SamplingDesign& sampling_design,
                                                      bool conditional_mode,
                                                      std::uint32_t bundle_size,
                                                      std::uint32_t frequency_threshold_pm,
                                                      std::uint32_t frequency_threshold_raw,
                                                      std::uint32_t document_dispersion_threshold,
                                                      std::uint32_t sample_count,
                                                      double coverage_target) {
    ExperimentCondition condition;
    condition.sampling_design_id = sampling_design.sampling_design_id;
    condition.condition_id = BuildConditionId(sampling_design,
                                              conditional_mode,
                                              bundle_size,
                                              frequency_threshold_pm,
                                              document_dispersion_threshold,
                                              coverage_target);
    condition.bundle_size = bundle_size;
    condition.conditional_mode = conditional_mode;
    condition.corpus_size_tokens = sampling_design.corpus_size_tokens;
    condition.corpus_delta_tokens = sampling_design.corpus_delta_tokens;
    condition.composition_label = sampling_design.composition_label;
    condition.frequency_threshold_pm = frequency_threshold_pm;
    condition.frequency_threshold_raw = frequency_threshold_raw;
    condition.document_dispersion_threshold = document_dispersion_threshold;
    condition.sample_count = sample_count;
    condition.coverage_target = coverage_target;
    return condition;
}

std::vector<ExperimentCondition> ExperimentEngine::build_conditions(const ExperimentOptions& options,
                                                                    const CorpusDataset& dataset) const {
    (void)dataset;
    SampleConditionPlanner planner;
    return planner.build_conditions(options);
}

void ExperimentEngine::emit_stage_progress(const ProgressEmitter* emitter,
                                           const std::string& stage,
                                           const std::string& message,
                                           int percent) const {
    if (emitter) {
        emitter->emit(stage + ": " + message, percent);
    }
}

std::string ExperimentEngine::join_path(const std::string& left, const std::string& right) {
    if (left.empty()) {
        return right;
    }
    if (left[left.size() - 1] == '/') {
        return left + right;
    }
    return left + "/" + right;
}

bool ExperimentEngine::file_exists(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

void ExperimentEngine::ensure_directory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        if (!S_ISDIR(st.st_mode)) {
            throw std::runtime_error("Path exists but is not a directory: " + path);
        }
        return;
    }
    if (mkdir(path.c_str(), 0755) != 0) {
        throw std::runtime_error("Failed to create directory: " + path + " (" + std::strerror(errno) + ")");
    }
}

void ExperimentEngine::ensure_directory_recursive(const std::string& path) {
    if (path.empty() || path == ".") {
        return;
    }
    std::string current;
    for (std::size_t i = 0; i < path.size(); ++i) {
        current.push_back(path[i]);
        if (path[i] == '/' || i + 1U == path.size()) {
            if (current.size() == 1U && current[0] == '/') {
                continue;
            }
            if (current[current.size() - 1U] == '/') {
                current.erase(current.size() - 1U);
            }
            if (!current.empty()) {
                ensure_directory(current);
            }
        }
    }
}

std::string ExperimentEngine::read_text_file(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void ExperimentEngine::write_json_file(const std::string& path, const nlohmann::json& payload) {
    std::ofstream output(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Failed to open output file: " + path);
    }
    output << payload.dump(2) << '\n';
}

std::string ExperimentEngine::timestamp_now() const {
    std::time_t now = std::time(0);
    std::tm tm_now;
    char buffer[64];
#if defined(_WIN32)
    tm_now = *std::localtime(&now);
#else
    localtime_r(&now, &tm_now);
#endif
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &tm_now) == 0) {
        return "1970-01-01T00:00:00+0000";
    }
    return buffer;
}

std::string ExperimentEngine::resolve_condition_filename(const std::string& prefix,
                                                         const ExperimentCondition& condition,
                                                         const std::string& suffix) {
    return prefix + condition.condition_id + suffix;
}

void ExperimentEngine::ensure_output_layout(const ExperimentOptions& options,
                                            std::string* artifact_dir,
                                            std::string* experiment_code) const {
    if (!artifact_dir || !experiment_code) {
        throw std::runtime_error("ensure_output_layout requires output pointers");
    }

    if (!options.artifact_input_dir.empty()) {
        *artifact_dir = options.artifact_input_dir;
        *experiment_code = options.requested_experiment_code.empty()
            ? "exp_resume"
            : options.requested_experiment_code;
        ensure_directory_recursive(*artifact_dir);
        return;
    }

    const std::string experiments_dir = join_path(options.dataset_dir, "experiments");
    ensure_directory_recursive(experiments_dir);

    std::string code = options.requested_experiment_code;
    if (code.empty()) {
        code = "exp_" + FormatUnsigned(options.random_seed);
    }

    std::string candidate_dir = join_path(experiments_dir, code);
    std::uint32_t suffix = 1U;
    while (!options.requested_experiment_code.empty() ? false : file_exists(join_path(candidate_dir, "run_manifest.json"))) {
        code = "exp_" + FormatUnsigned(options.random_seed) + "_" + FormatUnsigned(suffix);
        candidate_dir = join_path(experiments_dir, code);
        ++suffix;
    }

    ensure_directory_recursive(candidate_dir);
    *artifact_dir = candidate_dir;
    *experiment_code = code;
}

void ExperimentEngine::write_run_manifest(const ExperimentOptions& options,
                                          const std::vector<ExperimentCondition>& conditions) const {
    std::string artifact_dir;
    std::string experiment_code;
    ensure_output_layout(options, &artifact_dir, &experiment_code);

    nlohmann::json conditions_json = nlohmann::json::array();
    for (std::size_t i = 0; i < conditions.size(); ++i) {
        conditions_json.push_back(ToJson(conditions[i]));
    }

    ExperimentManifest manifest;
    manifest.experiment_code = experiment_code;
    manifest.experiment_title = options.experiment_title;
    manifest.artifact_dir = artifact_dir;
    manifest.run_mode = options.mode;
    manifest.created_at = timestamp_now();
    manifest.random_seed = options.random_seed;

    write_json_file(join_path(artifact_dir, "run_manifest.json"),
                    nlohmann::json{
                        {"manifest", ToJson(manifest)},
                        {"options", options.ToJson()},
                        {"conditions", conditions_json}
                    });
}

std::vector<ConditionRunResult> ExperimentEngine::run_sampling_designs(
    const CorpusDataset& dataset,
    const std::vector<ExperimentCondition>& conditions,
    const ExperimentOptions& options,
    const ProgressEmitter* progress_emitter) const {
    const std::string artifact_root = ArtifactRootFor(options);
    const std::string run_manifest_path = join_path(artifact_root, "run_manifest.json");

    SampleConditionPlanner planner;
    DocumentPoolBuilder pool_builder;
    CorpusSampler sampler(options.random_seed);
    ConditionalExtractor conditional_extractor(artifact_root, options.emit_debug_bundle_counts);
    UnconditionalExtractor unconditional_extractor(artifact_root, options.emit_debug_bundle_counts);
    RobustSetBuilder robust_builder;
    StabilityAnalyzer stability_analyzer;

    std::vector<ConditionRunResult> results;
    const std::vector<SamplingDesign> designs = planner.build_sampling_designs(options);

    for (std::size_t design_index = 0; design_index < designs.size(); ++design_index) {
        const SamplingDesign& design = designs[design_index];
        emit_stage_progress(progress_emitter,
                            "sampling",
                            "sampling design started: " + design.sampling_design_id,
                            static_cast<int>((100U * design_index) / std::max<std::size_t>(1U, designs.size() * 2U)));

        const CompositionConfig* composition = FindCompositionConfig(options, design.composition_label);
        if (!composition) {
            throw std::runtime_error("Missing composition config for label: " + design.composition_label);
        }
        const std::vector<DocumentInfo> pool = pool_builder.build_pool(dataset, *composition);
        const std::string membership_path = join_path(artifact_root, SampleMembershipArtifactName(design.sampling_design_id));
        std::vector<SampledCorpus> samples = sampler.resume_or_sample(membership_path, design, pool, 0);
        if (options.emit_sample_summary_json) {
            const SampleSummary summary = sampler.build_summary(design, samples, membership_path, run_manifest_path);
            sampler.write_summary_json(join_path(artifact_root, SampleSummaryArtifactName(design.sampling_design_id)),
                                       summary);
        }

        const std::vector<ExperimentCondition> design_conditions =
            ConditionsForDesign(conditions, design.sampling_design_id);
        for (std::size_t condition_index = 0; condition_index < design_conditions.size(); ++condition_index) {
            const ExperimentCondition& condition = design_conditions[condition_index];
            ConditionAggregate aggregate = condition.conditional_mode
                ? conditional_extractor.run(dataset, condition, samples, progress_emitter)
                : unconditional_extractor.run(dataset, condition, samples, progress_emitter);

            RobustBundleSet robust_set = robust_builder.build(condition, aggregate);
            const std::string robust_path = join_path(artifact_root, RobustSetArtifactName(condition));
            robust_builder.write(robust_path, condition, aggregate, robust_set);

            const StabilitySummary stability =
                stability_analyzer.analyze(condition, aggregate, robust_set);
            const std::string stability_path = join_path(artifact_root, StabilityArtifactName(condition));
            stability_analyzer.write(stability_path, stability);

            ConditionRunResult result;
            result.condition = condition;
            result.aggregate_artifact.artifact_type = "aggregate";
            result.aggregate_artifact.artifact_path = join_path(artifact_root, AggregateArtifactName(condition));
            result.aggregate_artifact.related_id = condition.condition_id;
            result.robust_set_artifact.artifact_type = "robust_set";
            result.robust_set_artifact.artifact_path = robust_path;
            result.robust_set_artifact.related_id = condition.condition_id;
            result.robust_set = robust_set;
            results.push_back(result);
        }
    }

    return results;
}

ExperimentRunResult ExperimentEngine::run_sample_only(const CorpusDataset& dataset,
                                                      const std::vector<ExperimentCondition>& conditions,
                                                      const ExperimentOptions& options,
                                                      const ProgressEmitter* progress_emitter) const {
    (void)conditions;
    ExperimentRunResult result;
    result.run_mode = options.mode;
    result.experiment_code = options.requested_experiment_code;
    result.artifact_dir = ArtifactRootFor(options);

    SampleConditionPlanner planner;
    DocumentPoolBuilder pool_builder;
    CorpusSampler sampler(options.random_seed);
    const std::string run_manifest_path = join_path(result.artifact_dir, "run_manifest.json");
    const std::vector<SamplingDesign> designs = planner.build_sampling_designs(options);

    for (std::size_t i = 0; i < designs.size(); ++i) {
        const SamplingDesign& design = designs[i];
        const CompositionConfig* composition = FindCompositionConfig(options, design.composition_label);
        if (!composition) {
            throw std::runtime_error("Missing composition config for label: " + design.composition_label);
        }
        const std::vector<DocumentInfo> pool = pool_builder.build_pool(dataset, *composition);
        const std::string membership_path = join_path(result.artifact_dir, SampleMembershipArtifactName(design.sampling_design_id));
        std::vector<SampledCorpus> samples = sampler.resume_or_sample(membership_path, design, pool, 0);
        const SampleSummary summary = sampler.build_summary(design, samples, membership_path, run_manifest_path);
        const std::string summary_path = join_path(result.artifact_dir, SampleSummaryArtifactName(design.sampling_design_id));
        sampler.write_summary_json(summary_path, summary);
        result.sample_summaries.push_back(summary);

        ArtifactDescriptor membership_artifact;
        membership_artifact.artifact_type = "sample_membership";
        membership_artifact.artifact_path = membership_path;
        membership_artifact.related_id = design.sampling_design_id;
        result.artifacts.push_back(membership_artifact);

        ArtifactDescriptor summary_artifact;
        summary_artifact.artifact_type = "sample_summary";
        summary_artifact.artifact_path = summary_path;
        summary_artifact.related_id = design.sampling_design_id;
        result.artifacts.push_back(summary_artifact);

        emit_stage_progress(progress_emitter,
                            "sampling",
                            "accepted sample count advanced for " + design.sampling_design_id,
                            static_cast<int>((100U * (i + 1U)) / std::max<std::size_t>(1U, designs.size())));
    }

    return result;
}

ExperimentRunResult ExperimentEngine::run_extract_only(const CorpusDataset& dataset,
                                                       const std::vector<ExperimentCondition>& conditions,
                                                       const ExperimentOptions& options,
                                                       const ProgressEmitter* progress_emitter) const {
    ExperimentRunResult result;
    result.run_mode = options.mode;
    result.experiment_code = options.requested_experiment_code;
    result.artifact_dir = ArtifactRootFor(options);

    SampleConditionPlanner planner;
    CorpusSampler sampler(options.random_seed);
    ConditionalExtractor conditional_extractor(result.artifact_dir, options.emit_debug_bundle_counts);
    UnconditionalExtractor unconditional_extractor(result.artifact_dir, options.emit_debug_bundle_counts);
    const std::vector<SamplingDesign> designs = planner.build_sampling_designs(options);

    for (std::size_t i = 0; i < designs.size(); ++i) {
        const SamplingDesign& design = designs[i];
        const std::string membership_path = join_path(result.artifact_dir, SampleMembershipArtifactName(design.sampling_design_id));
        SamplingDesign loaded_design;
        const std::vector<SampledCorpus> samples = sampler.load_membership_artifact(membership_path, &loaded_design);
        const std::vector<ExperimentCondition> design_conditions =
            ConditionsForDesign(conditions, design.sampling_design_id);

        for (std::size_t j = 0; j < design_conditions.size(); ++j) {
            const ExperimentCondition& condition = design_conditions[j];
            ConditionAggregate aggregate = condition.conditional_mode
                ? conditional_extractor.run(dataset, condition, samples, progress_emitter)
                : unconditional_extractor.run(dataset, condition, samples, progress_emitter);

            ConditionRunResult condition_result;
            condition_result.condition = condition;
            condition_result.aggregate_artifact.artifact_type = "aggregate";
            condition_result.aggregate_artifact.artifact_path =
                join_path(result.artifact_dir, AggregateArtifactName(condition));
            condition_result.aggregate_artifact.related_id = condition.condition_id;
            result.condition_results.push_back(condition_result);
            (void)aggregate;
        }
    }

    return result;
}

ExperimentRunResult ExperimentEngine::run_analyze_only(const CorpusDataset& dataset,
                                                       const std::vector<ExperimentCondition>& conditions,
                                                       const ExperimentOptions& options,
                                                       const ProgressEmitter* progress_emitter) const {
    (void)dataset;
    ExperimentRunResult result;
    result.run_mode = options.mode;
    result.experiment_code = options.requested_experiment_code;
    result.artifact_dir = ArtifactRootFor(options);

    ConditionalExtractor conditional_extractor(result.artifact_dir, options.emit_debug_bundle_counts);
    UnconditionalExtractor unconditional_extractor(result.artifact_dir, options.emit_debug_bundle_counts);
    RobustSetBuilder robust_builder;
    StabilityAnalyzer stability_analyzer;
    ComparisonAnalyzer comparison_analyzer;

    std::vector<RobustBundleSet> robust_sets;
    for (std::size_t i = 0; i < conditions.size(); ++i) {
        const ExperimentCondition& condition = conditions[i];
        const std::string aggregate_path = join_path(result.artifact_dir, AggregateArtifactName(condition));
        const ConditionAggregate aggregate = condition.conditional_mode
            ? conditional_extractor.load_aggregate_artifact(aggregate_path)
            : unconditional_extractor.load_aggregate_artifact(aggregate_path);

        const RobustBundleSet robust_set = robust_builder.build(condition, aggregate);
        const std::string robust_path = join_path(result.artifact_dir, RobustSetArtifactName(condition));
        robust_builder.write(robust_path, condition, aggregate, robust_set);

        const StabilitySummary stability = stability_analyzer.analyze(condition, aggregate, robust_set);
        stability_analyzer.write(join_path(result.artifact_dir, StabilityArtifactName(condition)), stability);

        ConditionRunResult condition_result;
        condition_result.condition = condition;
        condition_result.aggregate_artifact.artifact_type = "aggregate";
        condition_result.aggregate_artifact.artifact_path = aggregate_path;
        condition_result.aggregate_artifact.related_id = condition.condition_id;
        condition_result.robust_set_artifact.artifact_type = "robust_set";
        condition_result.robust_set_artifact.artifact_path = robust_path;
        condition_result.robust_set_artifact.related_id = condition.condition_id;
        condition_result.robust_set = robust_set;
        result.condition_results.push_back(condition_result);
        robust_sets.push_back(robust_set);
    }

    result.comparisons = comparison_analyzer.compare_all(robust_sets);
    comparison_analyzer.write(join_path(result.artifact_dir, "final_comparisons.json"), result.comparisons);
    emit_stage_progress(progress_emitter, "analysis", "robust set finalized", 100);
    return result;
}

ExperimentRunResult ExperimentEngine::run_sampling_diagnostics_only(
    const ExperimentOptions& options,
    const ProgressEmitter* progress_emitter) const {
    const std::string artifact_root = ArtifactRootFor(options);
    const ExperimentOptions manifest_options = LoadOptionsFromRunManifest(artifact_root);
    CorpusDataset dataset = open_dataset(manifest_options);

    ExperimentRunResult result;
    result.run_mode = options.mode;
    result.experiment_code = options.requested_experiment_code;
    result.artifact_dir = artifact_root;

    SampleConditionPlanner planner;
    DocumentPoolBuilder pool_builder;
    CorpusSampler sampler(manifest_options.random_seed);
    SamplingDiagnosticsBuilder diagnostics_builder;
    const std::vector<SamplingDesign> designs = planner.build_sampling_designs(manifest_options);

    for (std::size_t i = 0; i < designs.size(); ++i) {
        const SamplingDesign& design = designs[i];
        const CompositionConfig* composition = FindCompositionConfig(manifest_options, design.composition_label);
        if (!composition) {
            throw std::runtime_error("Missing composition config for label: " + design.composition_label);
        }

        const std::vector<DocumentInfo> pool = pool_builder.build_pool(dataset, *composition);
        const std::string membership_path =
            join_path(artifact_root, SampleMembershipArtifactName(design.sampling_design_id));
        SamplingDesign loaded_design;
        const std::vector<SampledCorpus> samples =
            sampler.load_membership_artifact(membership_path, &loaded_design);

        const nlohmann::json diagnostics =
            diagnostics_builder.build(loaded_design, pool, samples, 10000U);
        const std::string diagnostics_path =
            join_path(artifact_root, SamplingDiagnosticsArtifactName(design.sampling_design_id));
        diagnostics_builder.write(diagnostics_path, diagnostics);

        ArtifactDescriptor artifact;
        artifact.artifact_type = "sampling_diagnostics";
        artifact.artifact_path = diagnostics_path;
        artifact.related_id = design.sampling_design_id;
        result.artifacts.push_back(artifact);

        emit_stage_progress(progress_emitter,
                            "sampling_diagnostics",
                            "sampling diagnostics written for " + design.sampling_design_id,
                            static_cast<int>((100U * (i + 1U)) / std::max<std::size_t>(1U, designs.size())));
    }

    return result;
}

ExperimentRunResult ExperimentEngine::run_extraction_diagnostics_only(
    const ExperimentOptions& options,
    const ProgressEmitter* progress_emitter) const {
    const std::string artifact_root = ArtifactRootFor(options);
    const ExperimentOptions manifest_options = LoadOptionsFromRunManifest(artifact_root);
    CorpusDataset dataset = open_dataset(manifest_options);
    const std::vector<ExperimentCondition> conditions = build_conditions(manifest_options, dataset);

    ExperimentRunResult result;
    result.run_mode = options.mode;
    result.experiment_code = options.requested_experiment_code;
    result.artifact_dir = artifact_root;

    ConditionalExtractor conditional_extractor(artifact_root, false);
    ExtractionDiagnosticsBuilder diagnostics_builder;

    std::size_t completed = 0U;
    std::size_t total = 0U;
    for (std::size_t i = 0; i < conditions.size(); ++i) {
        if (conditions[i].conditional_mode) {
            ++total;
        }
    }

    for (std::size_t i = 0; i < conditions.size(); ++i) {
        const ExperimentCondition& condition = conditions[i];
        if (!condition.conditional_mode) {
            continue;
        }

        const std::string aggregate_path = join_path(artifact_root, AggregateArtifactName(condition));
        const ConditionAggregate aggregate = conditional_extractor.load_aggregate_artifact(aggregate_path);
        const std::string diagnostics_path = ExtractionDiagnosticsPath(artifact_root, condition);
        diagnostics_builder.write(diagnostics_path, diagnostics_builder.build(condition, aggregate));

        ArtifactDescriptor artifact;
        artifact.artifact_type = "extraction_diagnostics";
        artifact.artifact_path = diagnostics_path;
        artifact.related_id = condition.condition_id;
        result.artifacts.push_back(artifact);

        ++completed;
        emit_stage_progress(progress_emitter,
                            "extraction_diagnostics",
                            "extraction diagnostics written for " + condition.condition_id,
                            static_cast<int>((100U * completed) / std::max<std::size_t>(1U, total)));
    }

    return result;
}

void ExperimentEngine::write_final_comparison_artifact(const ExperimentRunResult& result) const {
    if (result.artifact_dir.empty()) {
        return;
    }
    ComparisonAnalyzer analyzer;
    analyzer.write(join_path(result.artifact_dir, "final_comparisons.json"), result.comparisons);
}

ExperimentRunResult ExperimentEngine::run_fun_mode(const ExperimentOptions& options,
                                                   const ProgressEmitter* progress_emitter) const {
    ExperimentOptions fun_options = options;
    fun_options.sample_count = fun_options.sample_count > 25U ? 25U : fun_options.sample_count;
    fun_options.random_seed = fun_options.random_seed == 0U ? 1U : fun_options.random_seed;
    fun_options.mode = RunMode::kRunExperiment;
    return run(fun_options, progress_emitter);
}

ExperimentRunResult ExperimentEngine::run(const ExperimentOptions& options,
                                          const ProgressEmitter* progress_emitter) const {
    validate_options(options);

    if (options.mode == RunMode::kFunRun) {
        return run_fun_mode(options, progress_emitter);
    }

    if (options.mode == RunMode::kInspectSamplingDiagnostics) {
        emit_stage_progress(progress_emitter, "sampling_diagnostics", "run started", 0);
        ExperimentRunResult diagnostics_result =
            run_sampling_diagnostics_only(options, progress_emitter);
        emit_stage_progress(progress_emitter, "sampling_diagnostics", "run completed", 100);
        return diagnostics_result;
    }

    if (options.mode == RunMode::kInspectExtractionDiagnostics) {
        emit_stage_progress(progress_emitter, "extraction_diagnostics", "run started", 0);
        ExperimentRunResult diagnostics_result =
            run_extraction_diagnostics_only(options, progress_emitter);
        emit_stage_progress(progress_emitter, "extraction_diagnostics", "run completed", 100);
        return diagnostics_result;
    }

    if (options.mode == RunMode::kInspectArtifacts) {
        ExperimentRunResult inspect_result;
        inspect_result.run_mode = options.mode;
        inspect_result.experiment_code = options.requested_experiment_code;
        inspect_result.artifact_dir = ArtifactRootFor(options);

        ArtifactInspector inspector;
        const std::vector<std::string> paths = ListDirectoryFiles(inspect_result.artifact_dir);
        for (std::size_t i = 0; i < paths.size(); ++i) {
            if (!IsJsonPath(paths[i])) {
                continue;
            }
            ArtifactInspectionResult inspection = inspector.inspect(paths[i]);
            ArtifactDescriptor artifact;
            artifact.artifact_type = inspection.artifact_type;
            artifact.artifact_path = inspection.artifact_path;
            artifact.related_id = "";
            inspect_result.artifacts.push_back(artifact);
            inspect_result.inspection_results.push_back(inspection);
        }
        emit_stage_progress(progress_emitter, "inspect", "artifact inspection completed", 100);
        return inspect_result;
    }

    emit_stage_progress(progress_emitter, "engine", "run started", 0);

    std::string artifact_dir;
    std::string experiment_code;
    ensure_output_layout(options, &artifact_dir, &experiment_code);

    ExperimentOptions effective_options = options;
    effective_options.requested_experiment_code = experiment_code;

    ensure_directory_recursive(artifact_dir);

    if (effective_options.artifact_input_dir.empty()) {
        MetadataRepository metadata(join_path(options.dataset_dir, "corpus.db"));
        metadata.ensure_experiment_registry();
        metadata.create_experiment_record(effective_options, artifact_dir, timestamp_now());
    }

    CorpusDataset dataset = open_dataset(effective_options);
    emit_stage_progress(progress_emitter, "engine", "dataset opened", 5);

    const std::vector<ExperimentCondition> conditions = build_conditions(effective_options, dataset);
    emit_stage_progress(progress_emitter, "engine", "conditions expanded", 10);

    write_run_manifest(effective_options, conditions);

    ExperimentRunResult result;
    result.experiment_code = experiment_code;
    result.artifact_dir = artifact_dir;
    result.run_mode = effective_options.mode;

    switch (effective_options.mode) {
        case RunMode::kRunExperiment:
            result.condition_results = run_sampling_designs(dataset, conditions, effective_options, progress_emitter);
            break;
        case RunMode::kSampleOnly:
            result = run_sample_only(dataset, conditions, effective_options, progress_emitter);
            break;
        case RunMode::kExtractOnly:
            result = run_extract_only(dataset, conditions, effective_options, progress_emitter);
            break;
        case RunMode::kAnalyzeOnly:
            result = run_analyze_only(dataset, conditions, effective_options, progress_emitter);
            break;
        case RunMode::kInspectSamplingDiagnostics:
        case RunMode::kInspectExtractionDiagnostics:
        case RunMode::kInspectArtifacts:
        case RunMode::kFunRun:
            break;
    }

    result.experiment_code = experiment_code;
    result.artifact_dir = artifact_dir;
    result.run_mode = effective_options.mode;

    if (effective_options.mode == RunMode::kRunExperiment || effective_options.mode == RunMode::kAnalyzeOnly) {
        std::vector<RobustBundleSet> robust_sets;
        for (std::size_t i = 0; i < result.condition_results.size(); ++i) {
            robust_sets.push_back(result.condition_results[i].robust_set);
        }
        ComparisonAnalyzer analyzer;
        result.comparisons = analyzer.compare_all(robust_sets);
        write_final_comparison_artifact(result);
    }

    if (effective_options.artifact_input_dir.empty()) {
        MetadataRepository metadata(join_path(options.dataset_dir, "corpus.db"));
        metadata.update_experiment_status(experiment_code, "completed", timestamp_now());
    }

    emit_stage_progress(progress_emitter, "engine", "run completed", 100);
    return result;
}

ExperimentRunResult ExperimentEngine::run_stepwise(const ExperimentOptions& options,
                                                   const ProgressEmitter* progress_emitter) const {
    return run(options, progress_emitter);
}

} // namespace teknegram
