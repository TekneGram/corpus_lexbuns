#include "stability_analyzer.hpp"

#include <cmath>
#include <fstream>
#include <stdexcept>

#include "result_formatter.hpp"

namespace teknegram {

StabilityAnalyzer::StabilityAnalyzer() {}

StabilitySummary StabilityAnalyzer::analyze(const ExperimentCondition& condition,
                                            const ConditionAggregate& aggregate,
                                            const RobustBundleSet& robust_set) const {
    StabilitySummary summary;
    summary.condition_id = condition.condition_id;
    summary.coverage_target = condition.coverage_target;
    summary.achieved_coverage = robust_set.achieved_coverage;
    summary.feature_count = static_cast<std::uint32_t>(aggregate.accumulated_mass.size());
    summary.stable_feature_count = static_cast<std::uint32_t>(robust_set.ranked_features.size());
    summary.top_feature_share = top_feature_share(robust_set.ranked_features);
    summary.normalized_entropy = entropy(robust_set.ranked_features);
    return summary;
}

nlohmann::json StabilityAnalyzer::to_json(const StabilitySummary& summary) const {
    return ResultFormatter::stability_to_json(summary);
}

void StabilityAnalyzer::write(const std::string& artifact_path,
                              const StabilitySummary& summary) const {
    std::ofstream output(artifact_path.c_str(), std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Failed to open stability artifact for writing: " + artifact_path);
    }
    output << to_json(summary).dump(2) << std::endl;
}

double StabilityAnalyzer::entropy(const std::vector<FeatureMassStat>& ranked_features) {
    const std::size_t feature_count = ranked_features.size();
    if (feature_count <= 1U) {
        return 0.0;
    }

    double total = 0.0;
    for (std::size_t i = 0; i < ranked_features.size(); ++i) {
        total += ranked_features[i].normalized_mass;
    }
    if (total <= 0.0) {
        return 0.0;
    }

    double value = 0.0;
    for (std::size_t i = 0; i < ranked_features.size(); ++i) {
        const double p = ranked_features[i].normalized_mass / total;
        if (p > 0.0) {
            value -= p * std::log(p);
        }
    }

    const double max_entropy = std::log(static_cast<double>(feature_count));
    if (max_entropy <= 0.0) {
        return 0.0;
    }
    return value / max_entropy;
}

double StabilityAnalyzer::top_feature_share(const std::vector<FeatureMassStat>& ranked_features) {
    if (ranked_features.empty()) {
        return 0.0;
    }
    return ranked_features.front().normalized_mass;
}

} // namespace teknegram
