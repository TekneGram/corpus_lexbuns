#include "robust_set_builder.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

#include "result_formatter.hpp"

namespace teknegram {

namespace {

bool FileExists(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    return static_cast<bool>(input);
}

} // namespace

RobustSetBuilder::RobustSetBuilder() {}

RobustBundleSet RobustSetBuilder::build(const ExperimentCondition& condition,
                                        const ConditionAggregate& aggregate) const {
    const double total_mass = compute_total_mass(aggregate);
    const std::vector<FeatureMassStat> ranked = rank_features(aggregate, total_mass);
    return select_prefix(condition, ranked);
}

void RobustSetBuilder::write(const std::string& artifact_path,
                             const ExperimentCondition& condition,
                             const ConditionAggregate& aggregate,
                             const RobustBundleSet& robust_set) const {
    std::ofstream output(artifact_path.c_str(), std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Failed to open robust set artifact for writing: " + artifact_path);
    }

    const double total_mass = compute_total_mass(aggregate);
    const nlohmann::json payload = ResultFormatter::format_robust_set(condition, aggregate, robust_set, total_mass);
    output << payload.dump(2) << std::endl;
}

RobustBundleSet RobustSetBuilder::load(const std::string& artifact_path) const {
    if (!FileExists(artifact_path)) {
        throw std::runtime_error("Robust set artifact not found: " + artifact_path);
    }

    std::ifstream input(artifact_path.c_str(), std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open robust set artifact: " + artifact_path);
    }

    nlohmann::json payload;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    try {
        payload = nlohmann::json::parse(buffer.str());
    } catch (const std::exception&) {
        throw std::runtime_error("Failed to parse robust set artifact: " + artifact_path);
    }

    return ResultFormatter::robust_set_from_json(payload);
}

std::vector<RobustBundleSet> RobustSetBuilder::load_many(const std::vector<std::string>& artifact_paths) const {
    std::vector<RobustBundleSet> result;
    for (std::size_t i = 0; i < artifact_paths.size(); ++i) {
        result.push_back(load(artifact_paths[i]));
    }
    return result;
}

double RobustSetBuilder::compute_total_mass(const ConditionAggregate& aggregate) {
    double total = 0.0;
    for (std::size_t i = 0; i < aggregate.accumulated_mass.size(); ++i) {
        total += aggregate.accumulated_mass[i];
    }
    return total;
}

std::vector<FeatureMassStat> RobustSetBuilder::rank_features(const ConditionAggregate& aggregate,
                                                             double total_mass) {
    std::vector<FeatureMassStat> ranked;
    ranked.reserve(aggregate.accumulated_mass.size());

    for (std::size_t i = 0; i < aggregate.accumulated_mass.size(); ++i) {
        FeatureMassStat stat;
        stat.feature_id = static_cast<FeatureId>(i);
        stat.expected_count = aggregate.sample_count == 0
            ? 0.0
            : aggregate.accumulated_mass[i] / static_cast<double>(aggregate.sample_count);
        stat.normalized_mass = total_mass > 0.0 ? aggregate.accumulated_mass[i] / total_mass : 0.0;
        stat.extracted_sample_count = i < aggregate.extracted_sample_counts.size()
            ? aggregate.extracted_sample_counts[i]
            : 0U;
        ranked.push_back(stat);
    }

    std::sort(ranked.begin(), ranked.end(), [](const FeatureMassStat& left, const FeatureMassStat& right) {
        if (left.normalized_mass != right.normalized_mass) {
            return left.normalized_mass > right.normalized_mass;
        }
        return left.feature_id < right.feature_id;
    });

    return ranked;
}

RobustBundleSet RobustSetBuilder::select_prefix(const ExperimentCondition& condition,
                                                const std::vector<FeatureMassStat>& ranked_features) {
    RobustBundleSet robust_set;
    robust_set.condition_id = condition.condition_id;
    robust_set.coverage_target = condition.coverage_target;

    double running_mass = 0.0;
    for (std::size_t i = 0; i < ranked_features.size(); ++i) {
        running_mass += ranked_features[i].normalized_mass;
        robust_set.ranked_features.push_back(ranked_features[i]);
        if (running_mass >= condition.coverage_target) {
            break;
        }
    }

    robust_set.achieved_coverage = running_mass;
    return robust_set;
}

} // namespace teknegram
