#include "json_io.hpp"

#include <stdexcept>

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

} // namespace

std::string RunModeToString(RunMode mode) {
    switch (mode) {
        case RunMode::kRunExperiment:
            return "runExperiment";
        case RunMode::kSampleOnly:
            return "sampleOnly";
        case RunMode::kExtractOnly:
            return "extractOnly";
        case RunMode::kAnalyzeOnly:
            return "analyzeOnly";
        case RunMode::kInspectSamplingDiagnostics:
            return "inspectSamplingDiagnostics";
        case RunMode::kInspectArtifacts:
            return "inspectArtifacts";
        case RunMode::kFunRun:
            return "funRun";
    }
    throw std::runtime_error("Unknown run mode enum");
}

RunMode ParseRunMode(const std::string& value) {
    if (value == "runExperiment") {
        return RunMode::kRunExperiment;
    }
    if (value == "sampleOnly") {
        return RunMode::kSampleOnly;
    }
    if (value == "extractOnly") {
        return RunMode::kExtractOnly;
    }
    if (value == "analyzeOnly") {
        return RunMode::kAnalyzeOnly;
    }
    if (value == "inspectSamplingDiagnostics") {
        return RunMode::kInspectSamplingDiagnostics;
    }
    if (value == "inspectArtifacts") {
        return RunMode::kInspectArtifacts;
    }
    if (value == "funRun") {
        return RunMode::kFunRun;
    }
    throw std::runtime_error("Unknown run mode: " + value);
}

std::string BuildSamplingDesignId(const CorpusSizeConfig& size_config,
                                  const CompositionConfig& composition_config) {
    return std::string("sz_") +
           std::to_string(size_config.target_tokens) +
           "_dl_" +
           std::to_string(size_config.delta_tokens) +
           "_cp_" +
           composition_config.label;
}

std::string BuildConditionId(const SamplingDesign& sampling_design,
                             bool conditional_mode,
                             std::uint32_t bundle_size,
                             std::uint32_t frequency_threshold_pm,
                             std::uint32_t document_dispersion_threshold,
                             double coverage_target) {
    std::string id = sampling_design.sampling_design_id;
    id += conditional_mode ? "_cond" : "_uncond";
    id += "_b" + std::to_string(bundle_size);
    if (conditional_mode) {
        id += "_f" + std::to_string(frequency_threshold_pm);
        id += "_d" + std::to_string(document_dispersion_threshold);
    }
    const std::uint32_t q_percent = static_cast<std::uint32_t>(coverage_target * 100.0 + 0.5);
    id += "_q" + std::to_string(q_percent);
    return id;
}

nlohmann::json ToJson(const CorpusSizeConfig& config) {
    return nlohmann::json{
        {"targetTokens", config.target_tokens},
        {"deltaTokens", config.delta_tokens}
    };
}

nlohmann::json ToJson(const CompositionConfig& config) {
    return nlohmann::json{
        {"label", config.label},
        {"minTokens", config.min_tokens},
        {"maxTokens", config.max_tokens}
    };
}

nlohmann::json ToJson(const SamplingDesign& design) {
    return nlohmann::json{
        {"samplingDesignId", design.sampling_design_id},
        {"corpusSizeTokens", design.corpus_size_tokens},
        {"corpusDeltaTokens", design.corpus_delta_tokens},
        {"compositionLabel", design.composition_label},
        {"sampleCount", design.sample_count},
        {"randomSeed", design.random_seed}
    };
}

nlohmann::json ToJson(const ExperimentCondition& condition) {
    return nlohmann::json{
        {"conditionId", condition.condition_id},
        {"samplingDesignId", condition.sampling_design_id},
        {"bundleSize", condition.bundle_size},
        {"conditionalMode", condition.conditional_mode},
        {"corpusSizeTokens", condition.corpus_size_tokens},
        {"corpusDeltaTokens", condition.corpus_delta_tokens},
        {"compositionLabel", condition.composition_label},
        {"frequencyThresholdPm", condition.frequency_threshold_pm},
        {"frequencyThresholdRaw", condition.frequency_threshold_raw},
        {"documentDispersionThreshold", condition.document_dispersion_threshold},
        {"sampleCount", condition.sample_count},
        {"coverageTarget", condition.coverage_target}
    };
}

nlohmann::json ToJson(const SampledCorpus& sample) {
    return nlohmann::json{
        {"sampleIndex", sample.sample_index},
        {"tokenCount", sample.token_count},
        {"documentIds", sample.document_ids}
    };
}

nlohmann::json ToJson(const SampleSummary& summary) {
    return nlohmann::json{
        {"samplingDesignId", summary.sampling_design_id},
        {"corpusSizeTokens", summary.corpus_size_tokens},
        {"corpusDeltaTokens", summary.corpus_delta_tokens},
        {"compositionLabel", summary.composition_label},
        {"randomSeed", summary.random_seed},
        {"targetSampleCount", summary.target_sample_count},
        {"acceptedSampleCount", summary.accepted_sample_count},
        {"meanSampleTokens", summary.mean_sample_tokens},
        {"minSampleTokens", summary.min_sample_tokens},
        {"maxSampleTokens", summary.max_sample_tokens},
        {"meanDocumentsPerSample", summary.mean_documents_per_sample},
        {"minDocumentsPerSample", summary.min_documents_per_sample},
        {"maxDocumentsPerSample", summary.max_documents_per_sample},
        {"sampleMembershipArtifactPath", summary.sample_membership_artifact_path},
        {"runManifestPath", summary.run_manifest_path}
    };
}

nlohmann::json ToJson(const FeatureMassStat& stat) {
    return nlohmann::json{
        {"featureId", stat.feature_id},
        {"normalizedMass", stat.normalized_mass},
        {"expectedCount", stat.expected_count},
        {"extractedSampleCount", stat.extracted_sample_count}
    };
}

nlohmann::json ToJson(const ConditionAggregate& aggregate) {
    return nlohmann::json{
        {"conditionId", aggregate.condition_id},
        {"sampleCount", aggregate.sample_count},
        {"conditionalMode", aggregate.conditional_mode},
        {"accumulatedMass", aggregate.accumulated_mass},
        {"extractedSampleCounts", aggregate.extracted_sample_counts}
    };
}

nlohmann::json ToJson(const RobustBundleSet& robust_set) {
    nlohmann::json ranked = nlohmann::json::array();
    for (std::size_t i = 0; i < robust_set.ranked_features.size(); ++i) {
        ranked.push_back(ToJson(robust_set.ranked_features[i]));
    }
    return nlohmann::json{
        {"conditionId", robust_set.condition_id},
        {"coverageTarget", robust_set.coverage_target},
        {"achievedCoverage", robust_set.achieved_coverage},
        {"rankedFeatures", ranked}
    };
}

nlohmann::json ToJson(const ArtifactDescriptor& artifact) {
    return nlohmann::json{
        {"artifactType", artifact.artifact_type},
        {"artifactPath", artifact.artifact_path},
        {"relatedId", artifact.related_id}
    };
}

nlohmann::json ToJson(const ComparisonScore& comparison) {
    return nlohmann::json{
        {"leftConditionId", comparison.left_condition_id},
        {"rightConditionId", comparison.right_condition_id},
        {"jaccard", comparison.jaccard}
    };
}

nlohmann::json ToJson(const ArtifactInspectionResult& inspection) {
    return nlohmann::json{
        {"artifactType", inspection.artifact_type},
        {"artifactPath", inspection.artifact_path},
        {"summary", inspection.summary_json}
    };
}

nlohmann::json ToJson(const ExperimentManifest& manifest) {
    return nlohmann::json{
        {"experimentCode", manifest.experiment_code},
        {"experimentTitle", manifest.experiment_title},
        {"artifactDir", manifest.artifact_dir},
        {"runMode", RunModeToString(manifest.run_mode)},
        {"createdAt", manifest.created_at},
        {"randomSeed", manifest.random_seed}
    };
}

nlohmann::json ToJson(const ConditionRunResult& result) {
    return nlohmann::json{
        {"condition", ToJson(result.condition)},
        {"aggregateArtifact", ToJson(result.aggregate_artifact)},
        {"robustSetArtifact", ToJson(result.robust_set_artifact)},
        {"robustSet", ToJson(result.robust_set)}
    };
}

nlohmann::json ToJson(const ExperimentRunResult& result) {
    nlohmann::json artifacts = nlohmann::json::array();
    for (std::size_t i = 0; i < result.artifacts.size(); ++i) {
        artifacts.push_back(ToJson(result.artifacts[i]));
    }

    nlohmann::json inspection_results = nlohmann::json::array();
    for (std::size_t i = 0; i < result.inspection_results.size(); ++i) {
        inspection_results.push_back(ToJson(result.inspection_results[i]));
    }

    nlohmann::json sample_summaries = nlohmann::json::array();
    for (std::size_t i = 0; i < result.sample_summaries.size(); ++i) {
        sample_summaries.push_back(ToJson(result.sample_summaries[i]));
    }

    nlohmann::json condition_results = nlohmann::json::array();
    for (std::size_t i = 0; i < result.condition_results.size(); ++i) {
        condition_results.push_back(ToJson(result.condition_results[i]));
    }

    nlohmann::json comparisons = nlohmann::json::array();
    for (std::size_t i = 0; i < result.comparisons.size(); ++i) {
        comparisons.push_back(ToJson(result.comparisons[i]));
    }

    return nlohmann::json{
        {"experimentCode", result.experiment_code},
        {"artifactDir", result.artifact_dir},
        {"runMode", RunModeToString(result.run_mode)},
        {"artifacts", artifacts},
        {"inspectionResults", inspection_results},
        {"sampleSummaries", sample_summaries},
        {"conditionResults", condition_results},
        {"comparisons", comparisons}
    };
}

CorpusSizeConfig CorpusSizeConfigFromJson(const nlohmann::json& value) {
    CorpusSizeConfig config;
    config.target_tokens = RequireUint32(value, "targetTokens");
    config.delta_tokens = RequireUint32(value, "deltaTokens");
    return config;
}

CompositionConfig CompositionConfigFromJson(const nlohmann::json& value) {
    CompositionConfig config;
    config.label = RequireString(value, "label");
    config.min_tokens = RequireUint32(value, "minTokens");
    config.max_tokens = RequireUint32(value, "maxTokens");
    return config;
}

ExperimentCondition ExperimentConditionFromJson(const nlohmann::json& value) {
    ExperimentCondition condition;
    condition.condition_id = RequireString(value, "conditionId");
    condition.sampling_design_id = RequireString(value, "samplingDesignId");
    condition.bundle_size = RequireUint32(value, "bundleSize");
    condition.conditional_mode = ReadBoolOrDefault(value, "conditionalMode", false);
    condition.corpus_size_tokens = RequireUint32(value, "corpusSizeTokens");
    condition.corpus_delta_tokens = RequireUint32(value, "corpusDeltaTokens");
    condition.composition_label = RequireString(value, "compositionLabel");
    condition.frequency_threshold_pm = RequireUint32(value, "frequencyThresholdPm");
    condition.frequency_threshold_raw = RequireUint32(value, "frequencyThresholdRaw");
    condition.document_dispersion_threshold = RequireUint32(value, "documentDispersionThreshold");
    condition.sample_count = RequireUint32(value, "sampleCount");
    condition.coverage_target = RequireDouble(value, "coverageTarget");
    return condition;
}

SampledCorpus SampledCorpusFromJson(const nlohmann::json& value) {
    SampledCorpus sample;
    sample.sample_index = RequireUint32(value, "sampleIndex");
    sample.token_count = RequireUint32(value, "tokenCount");
    if (!value.contains("documentIds") || !value["documentIds"].is_array()) {
        throw std::runtime_error("Missing or invalid documentIds array");
    }
    sample.document_ids = value["documentIds"].get<std::vector<DocId> >();
    return sample;
}

SampleSummary SampleSummaryFromJson(const nlohmann::json& value) {
    SampleSummary summary;
    summary.sampling_design_id = RequireString(value, "samplingDesignId");
    summary.corpus_size_tokens = RequireUint32(value, "corpusSizeTokens");
    summary.corpus_delta_tokens = RequireUint32(value, "corpusDeltaTokens");
    summary.composition_label = RequireString(value, "compositionLabel");
    summary.random_seed = RequireUint32(value, "randomSeed");
    summary.target_sample_count = RequireUint32(value, "targetSampleCount");
    summary.accepted_sample_count = RequireUint32(value, "acceptedSampleCount");
    summary.mean_sample_tokens = RequireDouble(value, "meanSampleTokens");
    summary.min_sample_tokens = RequireUint32(value, "minSampleTokens");
    summary.max_sample_tokens = RequireUint32(value, "maxSampleTokens");
    summary.mean_documents_per_sample = RequireDouble(value, "meanDocumentsPerSample");
    summary.min_documents_per_sample = RequireUint32(value, "minDocumentsPerSample");
    summary.max_documents_per_sample = RequireUint32(value, "maxDocumentsPerSample");
    summary.sample_membership_artifact_path = RequireString(value, "sampleMembershipArtifactPath");
    summary.run_manifest_path = RequireString(value, "runManifestPath");
    return summary;
}

ConditionAggregate ConditionAggregateFromJson(const nlohmann::json& value) {
    ConditionAggregate aggregate;
    aggregate.condition_id = RequireString(value, "conditionId");
    aggregate.sample_count = RequireUint32(value, "sampleCount");
    aggregate.conditional_mode = ReadBoolOrDefault(value, "conditionalMode", false);
    if (!value.contains("accumulatedMass") || !value["accumulatedMass"].is_array()) {
        throw std::runtime_error("Missing or invalid accumulatedMass array");
    }
    if (!value.contains("extractedSampleCounts") || !value["extractedSampleCounts"].is_array()) {
        throw std::runtime_error("Missing or invalid extractedSampleCounts array");
    }
    aggregate.accumulated_mass = value["accumulatedMass"].get<std::vector<double> >();
    aggregate.extracted_sample_counts =
        value["extractedSampleCounts"].get<std::vector<std::uint32_t> >();
    return aggregate;
}

RobustBundleSet RobustBundleSetFromJson(const nlohmann::json& value) {
    RobustBundleSet robust_set;
    robust_set.condition_id = RequireString(value, "conditionId");
    robust_set.coverage_target = RequireDouble(value, "coverageTarget");
    robust_set.achieved_coverage = RequireDouble(value, "achievedCoverage");
    if (!value.contains("rankedFeatures") || !value["rankedFeatures"].is_array()) {
        throw std::runtime_error("Missing or invalid rankedFeatures array");
    }
    const nlohmann::json& ranked = value["rankedFeatures"];
    for (nlohmann::json::size_type i = 0; i < ranked.size(); ++i) {
        FeatureMassStat stat;
        stat.feature_id = RequireUint32(ranked[i], "featureId");
        stat.normalized_mass = RequireDouble(ranked[i], "normalizedMass");
        stat.expected_count = RequireDouble(ranked[i], "expectedCount");
        stat.extracted_sample_count = RequireUint32(ranked[i], "extractedSampleCount");
        robust_set.ranked_features.push_back(stat);
    }
    return robust_set;
}

} // namespace teknegram
