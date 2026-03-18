#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../data_access_layer/corpus_dataset.hpp"
#include "../experiment_shared_layer/experiment_types.hpp"
#include "bundle_counter.hpp"
#include "threshold_policy.hpp"

namespace teknegram {

class ProgressEmitter;

struct SampleExtractionResult {
    std::uint32_t sample_index;
    std::vector<FeatureId> selected_feature_ids;
    std::vector<std::uint32_t> selected_raw_counts;
    std::vector<std::uint16_t> selected_document_ranges;

    SampleExtractionResult()
        : sample_index(0) {}
};

class ConditionalExtractor {
    public:
        explicit ConditionalExtractor(const std::string& artifact_root = std::string(),
                                      bool emit_debug_bundle_counts = false);

        ConditionAggregate run(const CorpusDataset& dataset,
                               const ExperimentCondition& condition,
                               const std::vector<SampledCorpus>& samples,
                               const ProgressEmitter* progress_emitter = 0) const;

        SampleExtractionResult extract_sample(const SampleBundleStats& stats,
                                              const ExperimentCondition& condition,
                                              const ThresholdPolicy& policy) const;

        void update_aggregate(const SampleBundleStats& stats,
                              const SampleExtractionResult& extracted,
                              ConditionAggregate* aggregate) const;

        void write_aggregate_artifact(const ExperimentCondition& condition,
                                      const ConditionAggregate& aggregate) const;

        ConditionAggregate load_aggregate_artifact(const std::string& artifact_path) const;

        void write_sample_level_sets_debug(const ExperimentCondition& condition,
                                          const std::vector<SampleExtractionResult>& sample_results) const;

    private:
        std::string artifact_root_;
        bool emit_debug_bundle_counts_;
};

} // namespace teknegram
