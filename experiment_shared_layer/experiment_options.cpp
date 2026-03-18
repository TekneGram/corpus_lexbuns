#include "experiment_options.hpp"

#include <cstdlib>
#include <stdexcept>

#include "json_io.hpp"

namespace teknegram {

namespace {

std::string RequireString(const nlohmann::json& input, const char* key) {
    if (!input.contains(key) || !input[key].is_string()) {
        throw std::runtime_error(std::string("Missing or invalid string field: ") + key);
    }
    return input[key].get<std::string>();
}

std::uint32_t RequireUint32(const nlohmann::json& input, const char* key) {
    if (!input.contains(key) || !input[key].is_number_unsigned()) {
        throw std::runtime_error(std::string("Missing or invalid unsigned field: ") + key);
    }
    return input[key].get<std::uint32_t>();
}

double RequireDouble(const nlohmann::json& input, const char* key) {
    if (!input.contains(key) || !input[key].is_number()) {
        throw std::runtime_error(std::string("Missing or invalid numeric field: ") + key);
    }
    return input[key].get<double>();
}

bool ReadBoolOrDefault(const nlohmann::json& input, const char* key, bool fallback) {
    if (!input.contains(key)) {
        return fallback;
    }
    if (!input[key].is_boolean()) {
        throw std::runtime_error(std::string("Invalid boolean field: ") + key);
    }
    return input[key].get<bool>();
}

std::string ReadStringOrDefault(const nlohmann::json& input,
                                const char* key,
                                const std::string& fallback) {
    if (!input.contains(key)) {
        return fallback;
    }
    if (!input[key].is_string()) {
        throw std::runtime_error(std::string("Invalid string field: ") + key);
    }
    return input[key].get<std::string>();
}

std::vector<std::uint32_t> ReadUint32Array(const nlohmann::json& input, const char* key) {
    if (!input.contains(key) || !input[key].is_array()) {
        throw std::runtime_error(std::string("Missing or invalid array field: ") + key);
    }
    std::vector<std::uint32_t> values;
    const nlohmann::json& array = input[key];
    for (nlohmann::json::size_type i = 0; i < array.size(); ++i) {
        if (!array[i].is_number_unsigned()) {
            throw std::runtime_error(std::string("Invalid unsigned element in array field: ") + key);
        }
        values.push_back(array[i].get<std::uint32_t>());
    }
    return values;
}

std::vector<CorpusSizeConfig> ReadSizeConfigs(const nlohmann::json& input) {
    if (!input.contains("sizeConfigs") || !input["sizeConfigs"].is_array()) {
        throw std::runtime_error("Missing or invalid array field: sizeConfigs");
    }
    std::vector<CorpusSizeConfig> values;
    const nlohmann::json& array = input["sizeConfigs"];
    for (nlohmann::json::size_type i = 0; i < array.size(); ++i) {
        values.push_back(CorpusSizeConfigFromJson(array[i]));
    }
    return values;
}

std::vector<CompositionConfig> ReadCompositionConfigs(const nlohmann::json& input) {
    if (!input.contains("compositionConfigs") || !input["compositionConfigs"].is_array()) {
        throw std::runtime_error("Missing or invalid array field: compositionConfigs");
    }
    std::vector<CompositionConfig> values;
    const nlohmann::json& array = input["compositionConfigs"];
    for (nlohmann::json::size_type i = 0; i < array.size(); ++i) {
        values.push_back(CompositionConfigFromJson(array[i]));
    }
    return values;
}

std::uint32_t ParseUnsignedArgument(const char* name, const std::string& value) {
    char* end = 0;
    const unsigned long parsed = std::strtoul(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0') {
        throw std::runtime_error(std::string("Invalid numeric value for ") + name + ": " + value);
    }
    return static_cast<std::uint32_t>(parsed);
}

double ParseDoubleArgument(const char* name, const std::string& value) {
    char* end = 0;
    const double parsed = std::strtod(value.c_str(), &end);
    if (end == value.c_str() || *end != '\0') {
        throw std::runtime_error(std::string("Invalid numeric value for ") + name + ": " + value);
    }
    return parsed;
}

std::vector<double> ReadCoverageTargets(const nlohmann::json& input) {
    std::vector<double> values;
    if (input.contains("coverageTargets")) {
        if (!input["coverageTargets"].is_array()) {
            throw std::runtime_error("Invalid array field: coverageTargets");
        }
        const nlohmann::json& array = input["coverageTargets"];
        for (nlohmann::json::size_type i = 0; i < array.size(); ++i) {
            if (!array[i].is_number()) {
                throw std::runtime_error("Invalid numeric element in array field: coverageTargets");
            }
            values.push_back(array[i].get<double>());
        }
        return values;
    }
    if (input.contains("coverageTarget")) {
        values.push_back(RequireDouble(input, "coverageTarget"));
        return values;
    }
    throw std::runtime_error("Missing coverageTarget or coverageTargets");
}

} // namespace

ExperimentOptions::ExperimentOptions()
    : mode(RunMode::kRunExperiment),
      sample_count(10000),
      bundle_size(4),
      coverage_target(0.9),
      random_seed(12345),
      run_conditional(true),
      run_unconditional(true),
      emit_sample_level_sets(false),
      emit_intermediate_artifacts(true),
      emit_debug_bundle_counts(false),
      emit_sample_summary_json(true) {
    coverage_targets.push_back(coverage_target);
}

ExperimentOptions ExperimentOptions::FromJson(const nlohmann::json& input) {
    ExperimentOptions options;
    options.dataset_dir = RequireString(input, "datasetDir");
    options.experiment_title = ReadStringOrDefault(input, "experimentTitle", "Lexical bundle experiment");
    options.mode = input.contains("mode")
        ? ParseRunMode(RequireString(input, "mode"))
        : RunMode::kRunExperiment;
    if (options.mode != RunMode::kInspectArtifacts) {
        options.size_configs = ReadSizeConfigs(input);
        options.composition_configs = ReadCompositionConfigs(input);
        options.frequency_thresholds_pm = ReadUint32Array(input, "frequencyThresholdsPm");
        options.document_dispersion_thresholds = ReadUint32Array(input, "documentDispersionThresholds");
        options.sample_count = RequireUint32(input, "sampleCount");
        options.bundle_size = RequireUint32(input, "bundleSize");
        options.coverage_targets = ReadCoverageTargets(input);
        options.coverage_target = options.coverage_targets.front();
        options.random_seed = RequireUint32(input, "randomSeed");
    }
    options.run_conditional = ReadBoolOrDefault(input, "runConditional", true);
    options.run_unconditional = ReadBoolOrDefault(input, "runUnconditional", true);
    options.emit_sample_level_sets = ReadBoolOrDefault(input, "emitSampleLevelSets", false);
    options.emit_intermediate_artifacts = ReadBoolOrDefault(input, "emitIntermediateArtifacts", true);
    options.emit_debug_bundle_counts = ReadBoolOrDefault(input, "emitDebugBundleCounts", false);
    options.emit_sample_summary_json = ReadBoolOrDefault(input, "emitSampleSummaryJson", true);
    options.artifact_input_dir = ReadStringOrDefault(input, "artifactInputDir", "");
    options.requested_experiment_code = ReadStringOrDefault(input, "requestedExperimentCode", "");
    options.Validate();
    return options;
}

ExperimentOptions ExperimentOptions::FromCli(int argc, char** argv) {
    ExperimentOptions options;
    options.dataset_dir = (argc > 1) ? argv[1] : ".";
    if (argc > 2) {
        options.mode = ParseRunMode(argv[2]);
    }
    if (argc > 3) {
        options.sample_count = ParseUnsignedArgument("sampleCount", argv[3]);
    }
    if (argc > 4) {
        options.bundle_size = ParseUnsignedArgument("bundleSize", argv[4]);
    }
    if (argc > 5) {
        options.coverage_target = ParseDoubleArgument("coverageTarget", argv[5]);
        options.coverage_targets.assign(1, options.coverage_target);
    }
    if (argc > 6) {
        options.random_seed = ParseUnsignedArgument("randomSeed", argv[6]);
    }

    CorpusSizeConfig default_size;
    default_size.target_tokens = 100000;
    default_size.delta_tokens = 5000;
    options.size_configs.push_back(default_size);

    CompositionConfig default_composition;
    default_composition.label = "default";
    default_composition.min_tokens = 0;
    default_composition.max_tokens = 0;
    options.composition_configs.push_back(default_composition);

    options.frequency_thresholds_pm.push_back(40);
    options.document_dispersion_thresholds.push_back(10);
    options.Validate();
    return options;
}

nlohmann::json ExperimentOptions::ToJson() const {
    nlohmann::json size_configs_json = nlohmann::json::array();
    for (std::size_t i = 0; i < size_configs.size(); ++i) {
        size_configs_json.push_back(teknegram::ToJson(size_configs[i]));
    }

    nlohmann::json composition_configs_json = nlohmann::json::array();
    for (std::size_t i = 0; i < composition_configs.size(); ++i) {
        composition_configs_json.push_back(teknegram::ToJson(composition_configs[i]));
    }

    return nlohmann::json{
        {"datasetDir", dataset_dir},
        {"experimentTitle", experiment_title},
        {"mode", RunModeToString(mode)},
        {"sizeConfigs", size_configs_json},
        {"compositionConfigs", composition_configs_json},
        {"frequencyThresholdsPm", frequency_thresholds_pm},
        {"documentDispersionThresholds", document_dispersion_thresholds},
        {"sampleCount", sample_count},
        {"bundleSize", bundle_size},
        {"coverageTarget", coverage_target},
        {"coverageTargets", coverage_targets},
        {"randomSeed", random_seed},
        {"runConditional", run_conditional},
        {"runUnconditional", run_unconditional},
        {"emitSampleLevelSets", emit_sample_level_sets},
        {"emitIntermediateArtifacts", emit_intermediate_artifacts},
        {"emitDebugBundleCounts", emit_debug_bundle_counts},
        {"emitSampleSummaryJson", emit_sample_summary_json},
        {"artifactInputDir", artifact_input_dir},
        {"requestedExperimentCode", requested_experiment_code}
    };
}

void ExperimentOptions::Validate() const {
    if (dataset_dir.empty()) {
        throw std::runtime_error("datasetDir must not be empty");
    }
    if (mode == RunMode::kInspectArtifacts) {
        return;
    }
    if (size_configs.empty()) {
        throw std::runtime_error("At least one size config is required");
    }
    if (composition_configs.empty()) {
        throw std::runtime_error("At least one composition config is required");
    }
    if (!run_conditional && !run_unconditional && mode != RunMode::kInspectArtifacts) {
        throw std::runtime_error("At least one extraction mode must be enabled");
    }
    if (sample_count == 0 && mode != RunMode::kInspectArtifacts) {
        throw std::runtime_error("sampleCount must be greater than zero");
    }
    if (bundle_size < 2) {
        throw std::runtime_error("bundleSize must be at least 2");
    }
    if (coverage_target <= 0.0 || coverage_target > 1.0) {
        throw std::runtime_error("coverageTarget must be in the interval (0, 1]");
    }
    if (coverage_targets.empty()) {
        throw std::runtime_error("At least one coverage target is required");
    }
    for (std::size_t i = 0; i < coverage_targets.size(); ++i) {
        if (coverage_targets[i] <= 0.0 || coverage_targets[i] > 1.0) {
            throw std::runtime_error("coverageTargets values must be in the interval (0, 1]");
        }
    }
}

} // namespace teknegram
