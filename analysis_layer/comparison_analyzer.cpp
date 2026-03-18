#include "comparison_analyzer.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <set>
#include <stdexcept>

#include "result_formatter.hpp"

namespace teknegram {

namespace {

bool FileExists(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    return static_cast<bool>(input);
}

} // namespace

ComparisonAnalyzer::ComparisonAnalyzer() {}

ComparisonScore ComparisonAnalyzer::compare(const RobustBundleSet& left,
                                            const RobustBundleSet& right) const {
    const std::vector<FeatureId> left_ids = feature_ids(left);
    const std::vector<FeatureId> right_ids = feature_ids(right);

    std::size_t i = 0;
    std::size_t j = 0;
    std::size_t intersection = 0;
    while (i < left_ids.size() && j < right_ids.size()) {
        if (left_ids[i] == right_ids[j]) {
            ++intersection;
            ++i;
            ++j;
        } else if (left_ids[i] < right_ids[j]) {
            ++i;
        } else {
            ++j;
        }
    }

    const std::size_t union_count = left_ids.size() + right_ids.size() - intersection;
    ComparisonScore score;
    score.left_condition_id = left.condition_id;
    score.right_condition_id = right.condition_id;
    score.jaccard = union_count == 0 ? 0.0 : static_cast<double>(intersection) / static_cast<double>(union_count);
    return score;
}

std::vector<ComparisonScore> ComparisonAnalyzer::compare_all(const std::vector<RobustBundleSet>& robust_sets) const {
    std::vector<ComparisonScore> scores;
    for (std::size_t i = 0; i < robust_sets.size(); ++i) {
        for (std::size_t j = i + 1; j < robust_sets.size(); ++j) {
            scores.push_back(compare(robust_sets[i], robust_sets[j]));
        }
    }
    return scores;
}

void ComparisonAnalyzer::write(const std::string& artifact_path,
                               const std::vector<ComparisonScore>& scores) const {
    std::ofstream output(artifact_path.c_str(), std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Failed to open comparison artifact for writing: " + artifact_path);
    }
    output << ResultFormatter::comparisons_to_json(scores).dump(2) << std::endl;
}

std::vector<ComparisonScore> ComparisonAnalyzer::load(const std::string& artifact_path) const {
    if (!FileExists(artifact_path)) {
        throw std::runtime_error("Comparison artifact not found: " + artifact_path);
    }

    std::ifstream input(artifact_path.c_str(), std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open comparison artifact: " + artifact_path);
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(buffer.str());
    } catch (const std::exception&) {
        throw std::runtime_error("Failed to parse comparison artifact: " + artifact_path);
    }

    std::vector<ComparisonScore> scores;
    if (!payload.is_object() || !payload.contains("comparisons") || !payload["comparisons"].is_array()) {
        throw std::runtime_error("Invalid comparison artifact format: " + artifact_path);
    }
    const nlohmann::json& comparisons = payload["comparisons"];
    for (nlohmann::json::size_type i = 0; i < comparisons.size(); ++i) {
        ComparisonScore score;
        score.left_condition_id = comparisons[i]["leftConditionId"].get<std::string>();
        score.right_condition_id = comparisons[i]["rightConditionId"].get<std::string>();
        score.jaccard = comparisons[i]["jaccard"].get<double>();
        scores.push_back(score);
    }
    return scores;
}

std::vector<FeatureId> ComparisonAnalyzer::feature_ids(const RobustBundleSet& robust_set) {
    std::vector<FeatureId> ids;
    ids.reserve(robust_set.ranked_features.size());
    for (std::size_t i = 0; i < robust_set.ranked_features.size(); ++i) {
        ids.push_back(robust_set.ranked_features[i].feature_id);
    }
    std::sort(ids.begin(), ids.end());
    ids.erase(std::unique(ids.begin(), ids.end()), ids.end());
    return ids;
}

} // namespace teknegram
