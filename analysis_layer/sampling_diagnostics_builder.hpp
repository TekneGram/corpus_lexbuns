#pragma once

#include <string>
#include <vector>

#include "../experiment_shared_layer/experiment_types.hpp"
#include "../experiment_shared_layer/json_io.hpp"

namespace teknegram {

std::string SamplingDiagnosticsArtifactName(const std::string& sampling_design_id);

class SamplingDiagnosticsBuilder {
    public:
        SamplingDiagnosticsBuilder();

        nlohmann::json build(const SamplingDesign& design,
                             const std::vector<DocumentInfo>& pool,
                             const std::vector<SampledCorpus>& samples,
                             std::uint32_t pair_sample_count = 10000U) const;

        void write(const std::string& artifact_path,
                   const nlohmann::json& payload) const;
};

} // namespace teknegram
