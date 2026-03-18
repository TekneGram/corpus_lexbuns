#pragma once

#include <string>
#include <vector>

#include "../experiment_shared_layer/experiment_types.hpp"
#include "../experiment_shared_layer/json_io.hpp"

namespace teknegram {

struct ComparisonArtifact {
    std::vector<ComparisonScore> scores;
};

class ComparisonAnalyzer {
    public:
        ComparisonAnalyzer();

        ComparisonScore compare(const RobustBundleSet& left,
                                const RobustBundleSet& right) const;
        std::vector<ComparisonScore> compare_all(const std::vector<RobustBundleSet>& robust_sets) const;
        void write(const std::string& artifact_path,
                   const std::vector<ComparisonScore>& scores) const;
        std::vector<ComparisonScore> load(const std::string& artifact_path) const;

    private:
        static std::vector<FeatureId> feature_ids(const RobustBundleSet& robust_set);
};

} // namespace teknegram
