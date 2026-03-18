#pragma once

#include <string>
#include <vector>

#include "../experiment_shared_layer/experiment_types.hpp"
#include "../experiment_shared_layer/json_io.hpp"
#include "stability_analyzer.hpp"

namespace teknegram {

class ResultFormatter {
    public:
        static nlohmann::json format_robust_set(const ExperimentCondition& condition,
                                                const ConditionAggregate& aggregate,
                                                const RobustBundleSet& robust_set,
                                                double total_mass);
        static nlohmann::json stability_to_json(const StabilitySummary& summary);
        static nlohmann::json comparisons_to_json(const std::vector<ComparisonScore>& scores);
        static nlohmann::json artifact_inspection_json(const ArtifactInspectionResult& result);

        static RobustBundleSet robust_set_from_json(const nlohmann::json& payload);
        static std::vector<ComparisonScore> comparisons_from_json(const nlohmann::json& payload);
        static StabilitySummary stability_from_json(const nlohmann::json& payload);
};

} // namespace teknegram
