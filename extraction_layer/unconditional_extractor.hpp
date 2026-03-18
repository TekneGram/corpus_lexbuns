#pragma once

#include <string>
#include <vector>

#include "../data_access_layer/corpus_dataset.hpp"
#include "../experiment_shared_layer/experiment_types.hpp"
#include "bundle_counter.hpp"

namespace teknegram {

class ProgressEmitter;

class UnconditionalExtractor {
    public:
        explicit UnconditionalExtractor(const std::string& artifact_root = std::string(),
                                        bool emit_debug_bundle_counts = false);

        ConditionAggregate run(const CorpusDataset& dataset,
                               const ExperimentCondition& condition,
                               const std::vector<SampledCorpus>& samples,
                               const ProgressEmitter* progress_emitter = 0) const;

        void update_aggregate(const SampleBundleStats& stats,
                              ConditionAggregate* aggregate) const;

        void write_aggregate_artifact(const ExperimentCondition& condition,
                                      const ConditionAggregate& aggregate) const;

        ConditionAggregate load_aggregate_artifact(const std::string& artifact_path) const;

    private:
        std::string artifact_root_;
        bool emit_debug_bundle_counts_;
};

} // namespace teknegram
