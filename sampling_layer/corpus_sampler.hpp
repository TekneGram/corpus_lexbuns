#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../data_access_layer/corpus_dataset.hpp"

namespace teknegram {

std::string SampleMembershipArtifactName(const std::string& sampling_design_id);
std::string SampleSummaryArtifactName(const std::string& sampling_design_id);

class CorpusSampler {
    public:
        CorpusSampler();
        explicit CorpusSampler(std::uint32_t random_seed);

        std::vector<SampledCorpus> build_samples(const SamplingDesign& design,
                                                 const std::vector<DocumentInfo>& pool) const;
        std::vector<SampledCorpus> build_samples(const CorpusDataset& dataset,
                                                 const SamplingDesign& design,
                                                 const std::vector<DocumentInfo>& pool) const;

        void write_membership_artifact(const std::string& path,
                                       const SamplingDesign& design,
                                       const std::vector<SampledCorpus>& samples) const;
        std::vector<SampledCorpus> load_membership_artifact(const std::string& path,
                                                            SamplingDesign* design_out = 0) const;

        void write_summary_json(const std::string& path, const SampleSummary& summary) const;
        SampleSummary load_summary_json(const std::string& path) const;
        SampleSummary build_summary(const SamplingDesign& design,
                                    const std::vector<SampledCorpus>& samples,
                                    const std::string& membership_artifact_path,
                                    const std::string& run_manifest_path) const;

        std::vector<SampledCorpus> resume_or_sample(const std::string& membership_artifact_path,
                                                    const SamplingDesign& design,
                                                    const std::vector<DocumentInfo>& pool,
                                                    bool* loaded_existing = 0) const;

    private:
        std::uint32_t random_seed_;
};

} // namespace teknegram
