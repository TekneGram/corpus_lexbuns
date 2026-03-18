#pragma once

#include <string>

#include "../experiment_shared_layer/experiment_types.hpp"
#include "../experiment_shared_layer/json_io.hpp"

namespace teknegram {

std::string ExtractionDiagnosticsArtifactName(const ExperimentCondition& condition);

class ExtractionDiagnosticsBuilder {
    public:
        ExtractionDiagnosticsBuilder();

        nlohmann::json build(const ExperimentCondition& condition,
                             const ConditionAggregate& aggregate) const;

        void write(const std::string& artifact_path,
                   const nlohmann::json& payload) const;
};

} // namespace teknegram
