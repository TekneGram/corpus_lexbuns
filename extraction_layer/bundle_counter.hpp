#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../data_access_layer/corpus_dataset.hpp"
#include "../experiment_shared_layer/experiment_types.hpp"

namespace teknegram {

class BundleCounter {
    public:
        explicit BundleCounter(const std::string& artifact_root = std::string(),
                               bool emit_debug_bundle_counts = false);

        SampleBundleStats count_sample(const CorpusDataset& dataset,
                                       const SampledCorpus& sample,
                                       std::uint32_t bundle_size) const;

        void accumulate_sample(const CorpusDataset& dataset,
                               const SampledCorpus& sample,
                               std::vector<std::uint32_t>* feature_counts,
                               std::vector<std::uint16_t>* feature_doc_ranges) const;

        void write_debug_bundle_counts(const ExperimentCondition& condition,
                                       const SampledCorpus& sample,
                                       const SampleBundleStats& stats) const;

    private:
        std::string artifact_root_;
        bool emit_debug_bundle_counts_;
};

} // namespace teknegram
