#pragma once

#include <string>

#include "../experiment_shared_layer/experiment_types.hpp"
#include "../experiment_shared_layer/json_io.hpp"

namespace teknegram {

struct StabilitySummary {
    std::string condition_id;
    double coverage_target;
    double achieved_coverage;
    std::uint32_t feature_count;
    std::uint32_t stable_feature_count;
    double top_feature_share;
    double normalized_entropy;
};

class StabilityAnalyzer {
    public:
        StabilityAnalyzer();

        StabilitySummary analyze(const ExperimentCondition& condition,
                                 const ConditionAggregate& aggregate,
                                 const RobustBundleSet& robust_set) const;
        nlohmann::json to_json(const StabilitySummary& summary) const;
        void write(const std::string& artifact_path,
                   const StabilitySummary& summary) const;

    private:
        static double entropy(const std::vector<FeatureMassStat>& ranked_features);
        static double top_feature_share(const std::vector<FeatureMassStat>& ranked_features);
};

} // namespace teknegram
