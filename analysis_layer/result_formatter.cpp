#include "result_formatter.hpp"

#include <stdexcept>

namespace teknegram {

nlohmann::json ResultFormatter::format_robust_set(const ExperimentCondition& condition,
                                                  const ConditionAggregate& aggregate,
                                                  const RobustBundleSet& robust_set,
                                                  double total_mass) {
    nlohmann::json ranked = nlohmann::json::array();
    for (std::size_t i = 0; i < robust_set.ranked_features.size(); ++i) {
        ranked.push_back(ToJson(robust_set.ranked_features[i]));
    }

    return nlohmann::json{
        {"artifactType", "robust_set"},
        {"condition", ToJson(condition)},
        {"aggregate", ToJson(aggregate)},
        {"robustSet", ToJson(robust_set)},
        {"rankedFeatures", ranked},
        {"totalMass", total_mass}
    };
}

nlohmann::json ResultFormatter::stability_to_json(const StabilitySummary& summary) {
    return nlohmann::json{
        {"artifactType", "stability"},
        {"conditionId", summary.condition_id},
        {"coverageTarget", summary.coverage_target},
        {"achievedCoverage", summary.achieved_coverage},
        {"featureCount", summary.feature_count},
        {"stableFeatureCount", summary.stable_feature_count},
        {"topFeatureShare", summary.top_feature_share},
        {"normalizedEntropy", summary.normalized_entropy}
    };
}

nlohmann::json ResultFormatter::comparisons_to_json(const std::vector<ComparisonScore>& scores) {
    nlohmann::json comparisons = nlohmann::json::array();
    for (std::size_t i = 0; i < scores.size(); ++i) {
        comparisons.push_back(ToJson(scores[i]));
    }
    return nlohmann::json{
        {"artifactType", "comparison"},
        {"comparisons", comparisons}
    };
}

nlohmann::json ResultFormatter::artifact_inspection_json(const ArtifactInspectionResult& result) {
    return nlohmann::json{
        {"artifactType", result.artifact_type},
        {"artifactPath", result.artifact_path},
        {"summary", result.summary_json}
    };
}

RobustBundleSet ResultFormatter::robust_set_from_json(const nlohmann::json& payload) {
    if (!payload.is_object() || !payload.contains("robustSet")) {
        throw std::runtime_error("Invalid robust set payload");
    }
    return RobustBundleSetFromJson(payload["robustSet"]);
}

std::vector<ComparisonScore> ResultFormatter::comparisons_from_json(const nlohmann::json& payload) {
    if (!payload.is_object() || !payload.contains("comparisons") || !payload["comparisons"].is_array()) {
        throw std::runtime_error("Invalid comparison payload");
    }
    std::vector<ComparisonScore> scores;
    const nlohmann::json& comparisons = payload["comparisons"];
    for (nlohmann::json::size_type i = 0; i < comparisons.size(); ++i) {
        ComparisonScore score;
        score.left_condition_id = comparisons[i].value("leftConditionId", "");
        score.right_condition_id = comparisons[i].value("rightConditionId", "");
        score.jaccard = comparisons[i].value("jaccard", 0.0);
        scores.push_back(score);
    }
    return scores;
}

StabilitySummary ResultFormatter::stability_from_json(const nlohmann::json& payload) {
    if (!payload.is_object() || !payload.contains("conditionId")) {
        throw std::runtime_error("Invalid stability payload");
    }
    StabilitySummary summary;
    summary.condition_id = payload.value("conditionId", "");
    summary.coverage_target = payload.value("coverageTarget", 0.0);
    summary.achieved_coverage = payload.value("achievedCoverage", 0.0);
    summary.feature_count = payload.value("featureCount", 0U);
    summary.stable_feature_count = payload.value("stableFeatureCount", 0U);
    summary.top_feature_share = payload.value("topFeatureShare", 0.0);
    summary.normalized_entropy = payload.value("normalizedEntropy", 0.0);
    return summary;
}

} // namespace teknegram
