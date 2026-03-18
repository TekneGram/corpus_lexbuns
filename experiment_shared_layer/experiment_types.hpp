#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace teknegram {

using DocId = std::uint32_t;
using FeatureId = std::uint32_t;

enum class RunMode {
    kRunExperiment,
    kSampleOnly,
    kExtractOnly,
    kAnalyzeOnly,
    kInspectArtifacts,
    kFunRun
};

struct CorpusSizeConfig {
    std::uint32_t target_tokens;
    std::uint32_t delta_tokens;

    CorpusSizeConfig()
        : target_tokens(0),
          delta_tokens(0) {}
};

struct CompositionConfig {
    std::string label;
    std::uint32_t min_tokens;
    std::uint32_t max_tokens;

    CompositionConfig()
        : min_tokens(0),
          max_tokens(0) {}
};

struct SamplingDesign {
    std::string sampling_design_id;
    std::uint32_t corpus_size_tokens;
    std::uint32_t corpus_delta_tokens;
    std::string composition_label;
    std::uint32_t sample_count;
    std::uint32_t random_seed;

    SamplingDesign()
        : corpus_size_tokens(0),
          corpus_delta_tokens(0),
          sample_count(0),
          random_seed(0) {}
};

struct ExperimentCondition {
    std::string condition_id;
    std::string sampling_design_id;
    std::uint32_t bundle_size;
    bool conditional_mode;
    std::uint32_t corpus_size_tokens;
    std::uint32_t corpus_delta_tokens;
    std::string composition_label;
    std::uint32_t frequency_threshold_pm;
    std::uint32_t frequency_threshold_raw;
    std::uint32_t document_dispersion_threshold;
    std::uint32_t sample_count;
    double coverage_target;

    ExperimentCondition()
        : bundle_size(0),
          conditional_mode(false),
          corpus_size_tokens(0),
          corpus_delta_tokens(0),
          frequency_threshold_pm(0),
          frequency_threshold_raw(0),
          document_dispersion_threshold(0),
          sample_count(0),
          coverage_target(0.0) {}
};

struct DocumentInfo {
    DocId document_id;
    std::uint32_t token_start;
    std::uint32_t token_end;
    std::uint32_t token_count;
    std::string title;
    std::string author;
    std::string relative_path;
    std::map<std::string, std::string> metadata;

    DocumentInfo()
        : document_id(0),
          token_start(0),
          token_end(0),
          token_count(0) {}
};

struct SampledCorpus {
    std::uint32_t sample_index;
    std::uint32_t token_count;
    std::vector<DocId> document_ids;

    SampledCorpus()
        : sample_index(0),
          token_count(0) {}
};

struct SampleSummary {
    std::string sampling_design_id;
    std::uint32_t corpus_size_tokens;
    std::uint32_t corpus_delta_tokens;
    std::string composition_label;
    std::uint32_t random_seed;
    std::uint32_t target_sample_count;
    std::uint32_t accepted_sample_count;
    double mean_sample_tokens;
    std::uint32_t min_sample_tokens;
    std::uint32_t max_sample_tokens;
    double mean_documents_per_sample;
    std::uint32_t min_documents_per_sample;
    std::uint32_t max_documents_per_sample;
    std::string sample_membership_artifact_path;
    std::string run_manifest_path;

    SampleSummary()
        : corpus_size_tokens(0),
          corpus_delta_tokens(0),
          random_seed(0),
          target_sample_count(0),
          accepted_sample_count(0),
          mean_sample_tokens(0.0),
          min_sample_tokens(0),
          max_sample_tokens(0),
          mean_documents_per_sample(0.0),
          min_documents_per_sample(0),
          max_documents_per_sample(0) {}
};

struct FeatureMassStat {
    FeatureId feature_id;
    double normalized_mass;
    double expected_count;
    std::uint32_t extracted_sample_count;

    FeatureMassStat()
        : feature_id(0),
          normalized_mass(0.0),
          expected_count(0.0),
          extracted_sample_count(0) {}
};

struct SampleBundleStats {
    std::uint32_t sample_index;
    std::vector<std::uint32_t> feature_counts;
    std::vector<std::uint32_t> document_ranges;

    SampleBundleStats()
        : sample_index(0) {}
};

struct ConditionAggregate {
    std::string condition_id;
    std::uint32_t sample_count;
    bool conditional_mode;
    std::vector<double> accumulated_mass;
    std::vector<std::uint32_t> extracted_sample_counts;

    ConditionAggregate()
        : sample_count(0),
          conditional_mode(false) {}
};

struct RobustBundleSet {
    std::string condition_id;
    double coverage_target;
    double achieved_coverage;
    std::vector<FeatureMassStat> ranked_features;

    RobustBundleSet()
        : coverage_target(0.0),
          achieved_coverage(0.0) {}
};

struct ArtifactDescriptor {
    std::string artifact_type;
    std::string artifact_path;
    std::string related_id;
};

struct ArtifactInspectionResult {
    std::string artifact_type;
    std::string artifact_path;
    std::string summary_json;
};

struct ComparisonScore {
    std::string left_condition_id;
    std::string right_condition_id;
    double jaccard;

    ComparisonScore()
        : jaccard(0.0) {}
};

struct ExperimentManifest {
    std::string experiment_code;
    std::string experiment_title;
    std::string artifact_dir;
    RunMode run_mode;
    std::string created_at;
    std::uint32_t random_seed;
};

struct ConditionRunResult {
    ExperimentCondition condition;
    ArtifactDescriptor aggregate_artifact;
    ArtifactDescriptor robust_set_artifact;
    RobustBundleSet robust_set;
};

struct ExperimentRunResult {
    std::string experiment_code;
    std::string artifact_dir;
    RunMode run_mode;
    std::vector<ArtifactDescriptor> artifacts;
    std::vector<ArtifactInspectionResult> inspection_results;
    std::vector<SampleSummary> sample_summaries;
    std::vector<ConditionRunResult> condition_results;
    std::vector<ComparisonScore> comparisons;
};

std::string RunModeToString(RunMode mode);
RunMode ParseRunMode(const std::string& value);
std::string BuildSamplingDesignId(const CorpusSizeConfig& size_config,
                                  const CompositionConfig& composition_config);
std::string BuildConditionId(const SamplingDesign& sampling_design,
                             bool conditional_mode,
                             std::uint32_t bundle_size,
                             std::uint32_t frequency_threshold_pm,
                             std::uint32_t document_dispersion_threshold,
                             double coverage_target);

} // namespace teknegram
