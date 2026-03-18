#include "extraction_diagnostics_builder.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace teknegram {

namespace {

struct RankedDiagnosticFeature {
    FeatureId feature_id;
    double normalized_mass;
    double pass_rate;
};

double Share(std::uint64_t part, std::uint64_t total) {
    return total == 0U ? 0.0 : static_cast<double>(part) / static_cast<double>(total);
}

std::vector<RankedDiagnosticFeature> RankNonZeroFeatures(const ConditionAggregate& aggregate) {
    double total_mass = 0.0;
    for (std::size_t i = 0; i < aggregate.accumulated_mass.size(); ++i) {
        total_mass += aggregate.accumulated_mass[i];
    }

    std::vector<RankedDiagnosticFeature> ranked;
    ranked.reserve(aggregate.accumulated_mass.size());
    for (std::size_t i = 0; i < aggregate.accumulated_mass.size(); ++i) {
        const double mass = aggregate.accumulated_mass[i];
        if (mass <= 0.0) {
            continue;
        }

        RankedDiagnosticFeature feature;
        feature.feature_id = static_cast<FeatureId>(i);
        feature.normalized_mass = total_mass <= 0.0 ? 0.0 : mass / total_mass;
        feature.pass_rate = aggregate.sample_count == 0U
            ? 0.0
            : static_cast<double>(i < aggregate.extracted_sample_counts.size()
                                       ? aggregate.extracted_sample_counts[i]
                                       : 0U) /
                  static_cast<double>(aggregate.sample_count);
        ranked.push_back(feature);
    }

    std::sort(ranked.begin(), ranked.end(),
              [](const RankedDiagnosticFeature& left, const RankedDiagnosticFeature& right) {
                  if (left.normalized_mass != right.normalized_mass) {
                      return left.normalized_mass > right.normalized_mass;
                  }
                  return left.feature_id < right.feature_id;
              });
    return ranked;
}

std::uint32_t CoverageCrossedAtRank(const std::vector<RankedDiagnosticFeature>& ranked,
                                    double coverage_target) {
    double cumulative = 0.0;
    for (std::size_t i = 0; i < ranked.size(); ++i) {
        cumulative += ranked[i].normalized_mass;
        if (cumulative >= coverage_target) {
            return static_cast<std::uint32_t>(i + 1U);
        }
    }
    return static_cast<std::uint32_t>(ranked.size());
}

nlohmann::json BuildRetainedMassHistogram(const ExtractionTelemetry& telemetry) {
    const std::size_t bin_count = telemetry.retained_mass_share_histogram.size();
    const double bin_width = bin_count == 0U ? 0.0 : 1.0 / static_cast<double>(bin_count);
    nlohmann::json bins = nlohmann::json::array();
    for (std::size_t i = 0; i < bin_count; ++i) {
        const double start = static_cast<double>(i) * bin_width;
        const double end = (i + 1U == bin_count) ? 1.0 : start + bin_width;
        bins.push_back(nlohmann::json{
            {"start", start},
            {"end", end},
            {"sampleCount", telemetry.retained_mass_share_histogram[i]}
        });
    }
    return nlohmann::json{
        {"binWidth", bin_width},
        {"bins", bins}
    };
}

nlohmann::json BuildRankMassProfile(const ExperimentCondition& condition,
                                    const ConditionAggregate& aggregate) {
    const std::vector<RankedDiagnosticFeature> ranked = RankNonZeroFeatures(aggregate);
    nlohmann::json buckets = nlohmann::json::array();

    double cumulative = 0.0;
    std::size_t index = 0U;
    const std::size_t exact_limit = std::min<std::size_t>(10U, ranked.size());
    while (index < exact_limit) {
        cumulative += ranked[index].normalized_mass;
        buckets.push_back(nlohmann::json{
            {"rankStart", static_cast<std::uint32_t>(index + 1U)},
            {"rankEnd", static_cast<std::uint32_t>(index + 1U)},
            {"featureCount", 1U},
            {"massShare", ranked[index].normalized_mass},
            {"cumulativeMassEnd", cumulative},
            {"meanPassRate", ranked[index].pass_rate}
        });
        ++index;
    }

    std::size_t bucket_width = 10U;
    while (index < ranked.size()) {
        const std::size_t start = index;
        const std::size_t end = std::min(ranked.size(), start + bucket_width);
        double mass_share = 0.0;
        double total_pass_rate = 0.0;
        for (std::size_t i = start; i < end; ++i) {
            mass_share += ranked[i].normalized_mass;
            total_pass_rate += ranked[i].pass_rate;
        }
        cumulative += mass_share;
        buckets.push_back(nlohmann::json{
            {"rankStart", static_cast<std::uint32_t>(start + 1U)},
            {"rankEnd", static_cast<std::uint32_t>(end)},
            {"featureCount", static_cast<std::uint32_t>(end - start)},
            {"massShare", mass_share},
            {"cumulativeMassEnd", cumulative},
            {"meanPassRate", (end == start) ? 0.0 :
                total_pass_rate / static_cast<double>(end - start)}
        });
        index = end;
        if (bucket_width < 1000U) {
            bucket_width *= 2U;
        }
    }

    return nlohmann::json{
        {"nonZeroFeatureCount", static_cast<std::uint32_t>(ranked.size())},
        {"coverageTarget", condition.coverage_target},
        {"coverageCrossedAtRank", CoverageCrossedAtRank(ranked, condition.coverage_target)},
        {"buckets", buckets}
    };
}

} // namespace

std::string ExtractionDiagnosticsArtifactName(const ExperimentCondition& condition) {
    return std::string("extraction_diagnostics_") + condition.condition_id + ".json";
}

ExtractionDiagnosticsBuilder::ExtractionDiagnosticsBuilder() {}

nlohmann::json ExtractionDiagnosticsBuilder::build(const ExperimentCondition& condition,
                                                   const ConditionAggregate& aggregate) const {
    const ExtractionTelemetry& telemetry = aggregate.extraction_telemetry;
    const std::uint64_t total_observed_mass = telemetry.total_observed_mass;

    return nlohmann::json{
        {"artifactType", "extraction_diagnostics"},
        {"condition", ToJson(condition)},
        {"retainedMassShareBySample", BuildRetainedMassHistogram(telemetry)},
        {"filterCauseMassSplit", {
            {"passedShare", Share(telemetry.total_passed_mass, total_observed_mass)},
            {"failedFrequencyOnlyShare", Share(telemetry.failed_frequency_only_mass, total_observed_mass)},
            {"failedDispersionOnlyShare", Share(telemetry.failed_dispersion_only_mass, total_observed_mass)},
            {"failedBothShare", Share(telemetry.failed_both_mass, total_observed_mass)}
        }},
        {"rankMassStabilityProfile", BuildRankMassProfile(condition, aggregate)}
    };
}

void ExtractionDiagnosticsBuilder::write(const std::string& artifact_path,
                                         const nlohmann::json& payload) const {
    std::ofstream output(artifact_path.c_str(), std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Failed to open extraction diagnostics artifact for writing: " +
                                 artifact_path);
    }
    output << payload.dump(2) << std::endl;
}

} // namespace teknegram
