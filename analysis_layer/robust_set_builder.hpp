#pragma once

#include <string>
#include <vector>

#include "../experiment_shared_layer/experiment_types.hpp"
#include "../experiment_shared_layer/json_io.hpp"

namespace teknegram {

struct RobustSetArtifact {
    RobustBundleSet robust_set;
    ExperimentCondition condition;
    double total_mass;
};

class RobustSetBuilder {
    public:
        RobustSetBuilder();

        RobustBundleSet build(const ExperimentCondition& condition,
                              const ConditionAggregate& aggregate) const;
        void write(const std::string& artifact_path,
                   const ExperimentCondition& condition,
                   const ConditionAggregate& aggregate,
                   const RobustBundleSet& robust_set) const;
        RobustBundleSet load(const std::string& artifact_path) const;
        std::vector<RobustBundleSet> load_many(const std::vector<std::string>& artifact_paths) const;

    private:
        static double compute_total_mass(const ConditionAggregate& aggregate);
        static std::vector<FeatureMassStat> rank_features(const ConditionAggregate& aggregate,
                                                          double total_mass);
        static RobustBundleSet select_prefix(const ExperimentCondition& condition,
                                             const std::vector<FeatureMassStat>& ranked_features);
};

} // namespace teknegram
