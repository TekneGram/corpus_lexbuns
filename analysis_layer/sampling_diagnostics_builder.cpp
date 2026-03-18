#include "sampling_diagnostics_builder.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <unordered_map>

namespace teknegram {

namespace {

struct InclusionRecord {
    DocId document_id;
    std::string title;
    std::string relative_path;
    std::uint32_t token_count;
    std::uint32_t inclusion_count;
};

std::vector<std::uint32_t> SortedCounts(const std::vector<std::uint32_t>& counts) {
    std::vector<std::uint32_t> sorted = counts;
    std::sort(sorted.begin(), sorted.end());
    return sorted;
}

double Mean(const std::vector<std::uint32_t>& values) {
    if (values.empty()) {
        return 0.0;
    }
    const double total = std::accumulate(values.begin(), values.end(), 0.0);
    return total / static_cast<double>(values.size());
}

double Median(const std::vector<std::uint32_t>& sorted_values) {
    if (sorted_values.empty()) {
        return 0.0;
    }
    const std::size_t mid = sorted_values.size() / 2U;
    if (sorted_values.size() % 2U == 0U) {
        return (static_cast<double>(sorted_values[mid - 1U]) +
                static_cast<double>(sorted_values[mid])) / 2.0;
    }
    return static_cast<double>(sorted_values[mid]);
}

double StandardDeviation(const std::vector<std::uint32_t>& values, double mean) {
    if (values.empty()) {
        return 0.0;
    }
    double total = 0.0;
    for (std::size_t i = 0; i < values.size(); ++i) {
        const double diff = static_cast<double>(values[i]) - mean;
        total += diff * diff;
    }
    return std::sqrt(total / static_cast<double>(values.size()));
}

nlohmann::json BuildIntegerHistogram(const std::vector<std::uint32_t>& sorted_values,
                                     std::uint32_t bin_width) {
    nlohmann::json bins = nlohmann::json::array();
    if (sorted_values.empty()) {
        return nlohmann::json{{"binWidth", bin_width}, {"bins", bins}};
    }

    const std::uint32_t min_value = sorted_values.front();
    const std::uint32_t max_value = sorted_values.back();
    const std::uint32_t start = (min_value / bin_width) * bin_width;

    for (std::uint32_t bin_start = start; bin_start <= max_value; bin_start += bin_width) {
        const std::uint32_t bin_end = bin_start + bin_width - 1U;
        bins.push_back(nlohmann::json{
            {"start", bin_start},
            {"end", bin_end},
            {"documentCount", 0U}
        });
        if (bin_end >= max_value) {
            break;
        }
    }

    for (std::size_t i = 0; i < sorted_values.size(); ++i) {
        const std::uint32_t value = sorted_values[i];
        const std::size_t index = static_cast<std::size_t>((value - start) / bin_width);
        bins[index]["documentCount"] =
            bins[index]["documentCount"].get<std::uint32_t>() + 1U;
    }

    return nlohmann::json{{"binWidth", bin_width}, {"bins", bins}};
}

std::vector<InclusionRecord> BuildInclusionRecords(const std::vector<DocumentInfo>& pool,
                                                   const std::vector<std::uint32_t>& inclusion_counts) {
    std::vector<InclusionRecord> records;
    records.reserve(pool.size());
    for (std::size_t i = 0; i < pool.size(); ++i) {
        InclusionRecord record;
        record.document_id = pool[i].document_id;
        record.title = pool[i].title;
        record.relative_path = pool[i].relative_path;
        record.token_count = pool[i].token_count;
        record.inclusion_count = i < inclusion_counts.size() ? inclusion_counts[i] : 0U;
        records.push_back(record);
    }
    return records;
}

nlohmann::json RecordToJson(const InclusionRecord& record) {
    return nlohmann::json{
        {"documentId", record.document_id},
        {"title", record.title},
        {"relativePath", record.relative_path},
        {"tokenCount", record.token_count},
        {"inclusionCount", record.inclusion_count}
    };
}

nlohmann::json ExtremesToJson(std::vector<InclusionRecord> records, std::size_t limit) {
    std::sort(records.begin(), records.end(), [](const InclusionRecord& left, const InclusionRecord& right) {
        if (left.inclusion_count != right.inclusion_count) {
            return left.inclusion_count < right.inclusion_count;
        }
        return left.document_id < right.document_id;
    });

    nlohmann::json least = nlohmann::json::array();
    nlohmann::json most = nlohmann::json::array();

    const std::size_t least_limit = std::min(limit, records.size());
    for (std::size_t i = 0; i < least_limit; ++i) {
        least.push_back(RecordToJson(records[i]));
    }

    const std::size_t most_limit = std::min(limit, records.size());
    for (std::size_t i = 0; i < most_limit; ++i) {
        most.push_back(RecordToJson(records[records.size() - 1U - i]));
    }

    return nlohmann::json{
        {"leastIncludedDocuments", least},
        {"mostIncludedDocuments", most}
    };
}

double PearsonCorrelation(const std::vector<double>& left, const std::vector<double>& right) {
    if (left.size() != right.size() || left.empty()) {
        return 0.0;
    }

    const double left_mean = std::accumulate(left.begin(), left.end(), 0.0) / static_cast<double>(left.size());
    const double right_mean = std::accumulate(right.begin(), right.end(), 0.0) / static_cast<double>(right.size());

    double numerator = 0.0;
    double left_total = 0.0;
    double right_total = 0.0;
    for (std::size_t i = 0; i < left.size(); ++i) {
        const double left_diff = left[i] - left_mean;
        const double right_diff = right[i] - right_mean;
        numerator += left_diff * right_diff;
        left_total += left_diff * left_diff;
        right_total += right_diff * right_diff;
    }

    if (left_total <= 0.0 || right_total <= 0.0) {
        return 0.0;
    }
    return numerator / std::sqrt(left_total * right_total);
}

nlohmann::json BuildLengthBiasSummary(const std::vector<DocumentInfo>& pool,
                                      const std::vector<std::uint32_t>& inclusion_counts) {
    nlohmann::json bins = nlohmann::json::array();
    if (pool.empty()) {
        return nlohmann::json{
            {"pearsonCorrelationTokenCountVsInclusionCount", 0.0},
            {"lengthBins", bins}
        };
    }

    std::vector<double> token_counts;
    std::vector<double> inclusion_as_double;
    token_counts.reserve(pool.size());
    inclusion_as_double.reserve(pool.size());

    std::uint32_t min_tokens = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t max_tokens = 0U;
    for (std::size_t i = 0; i < pool.size(); ++i) {
        token_counts.push_back(static_cast<double>(pool[i].token_count));
        inclusion_as_double.push_back(static_cast<double>(inclusion_counts[i]));
        if (pool[i].token_count < min_tokens) {
            min_tokens = pool[i].token_count;
        }
        if (pool[i].token_count > max_tokens) {
            max_tokens = pool[i].token_count;
        }
    }

    const std::uint32_t bin_count = std::min<std::uint32_t>(5U, static_cast<std::uint32_t>(pool.size()));
    const std::uint32_t width = std::max<std::uint32_t>(1U,
        static_cast<std::uint32_t>((max_tokens - min_tokens + 1U + bin_count - 1U) / bin_count));

    for (std::uint32_t bin_index = 0; bin_index < bin_count; ++bin_index) {
        const std::uint32_t start = min_tokens + (bin_index * width);
        const std::uint32_t end = (bin_index + 1U == bin_count)
            ? max_tokens
            : (start + width - 1U);

        std::uint32_t document_count = 0U;
        double total_tokens = 0.0;
        double total_inclusions = 0.0;
        for (std::size_t i = 0; i < pool.size(); ++i) {
            const std::uint32_t token_count = pool[i].token_count;
            if (token_count < start || token_count > end) {
                continue;
            }
            ++document_count;
            total_tokens += static_cast<double>(token_count);
            total_inclusions += static_cast<double>(inclusion_counts[i]);
        }

        bins.push_back(nlohmann::json{
            {"label", std::to_string(start) + "-" + std::to_string(end)},
            {"documentCount", document_count},
            {"meanTokenCount", document_count == 0U ? 0.0 : total_tokens / static_cast<double>(document_count)},
            {"meanInclusionCount", document_count == 0U ? 0.0 : total_inclusions / static_cast<double>(document_count)}
        });
    }

    return nlohmann::json{
        {"pearsonCorrelationTokenCountVsInclusionCount", PearsonCorrelation(token_counts, inclusion_as_double)},
        {"lengthBins", bins}
    };
}

std::size_t OverlapCount(const std::vector<DocId>& left, const std::vector<DocId>& right) {
    std::size_t i = 0U;
    std::size_t j = 0U;
    std::size_t overlap = 0U;
    while (i < left.size() && j < right.size()) {
        if (left[i] == right[j]) {
            ++overlap;
            ++i;
            ++j;
        } else if (left[i] < right[j]) {
            ++i;
        } else {
            ++j;
        }
    }
    return overlap;
}

nlohmann::json BuildDoubleHistogram(const std::vector<double>& values,
                                    double bin_width) {
    nlohmann::json bins = nlohmann::json::array();
    if (values.empty() || bin_width <= 0.0) {
        return nlohmann::json{{"binWidth", bin_width}, {"bins", bins}};
    }

    double min_value = values[0];
    double max_value = values[0];
    for (std::size_t i = 1; i < values.size(); ++i) {
        min_value = std::min(min_value, values[i]);
        max_value = std::max(max_value, values[i]);
    }

    const double start = std::floor(min_value / bin_width) * bin_width;
    const std::size_t bin_count = static_cast<std::size_t>(std::floor((max_value - start) / bin_width)) + 1U;
    for (std::size_t i = 0; i < bin_count; ++i) {
        const double bin_start = start + (static_cast<double>(i) * bin_width);
        const double bin_end = bin_start + bin_width;
        bins.push_back(nlohmann::json{
            {"start", bin_start},
            {"end", bin_end},
            {"pairCount", 0U}
        });
    }

    for (std::size_t i = 0; i < values.size(); ++i) {
        const std::size_t index = static_cast<std::size_t>(std::floor((values[i] - start) / bin_width));
        bins[index]["pairCount"] = bins[index]["pairCount"].get<std::uint32_t>() + 1U;
    }

    return nlohmann::json{{"binWidth", bin_width}, {"bins", bins}};
}

nlohmann::json BuildSampleOverlapSummary(const std::vector<SampledCorpus>& samples,
                                         std::uint32_t requested_pair_sample_count,
                                         std::uint32_t random_seed) {
    if (samples.size() < 2U) {
        return nlohmann::json{
            {"pairSampleCount", 0U},
            {"documentOverlapCount", {{"mean", 0.0}, {"median", 0.0}, {"min", 0U}, {"max", 0U}}},
            {"jaccard", {{"mean", 0.0}, {"median", 0.0}, {"min", 0.0}, {"max", 0.0}}},
            {"jaccardHistogram", {{"binWidth", 0.005}, {"bins", nlohmann::json::array()}}}
        };
    }

    std::vector<std::vector<DocId> > sorted_samples(samples.size());
    for (std::size_t i = 0; i < samples.size(); ++i) {
        sorted_samples[i] = samples[i].document_ids;
        std::sort(sorted_samples[i].begin(), sorted_samples[i].end());
    }

    const std::uint32_t pair_sample_count = requested_pair_sample_count == 0U
        ? 10000U
        : requested_pair_sample_count;

    std::mt19937 rng(random_seed ^ 0x9E3779B9U);
    std::uniform_int_distribution<std::uint32_t> dist(0U, static_cast<std::uint32_t>(samples.size() - 1U));

    std::vector<std::uint32_t> overlaps;
    std::vector<double> jaccards;
    overlaps.reserve(pair_sample_count);
    jaccards.reserve(pair_sample_count);

    for (std::uint32_t pair_index = 0; pair_index < pair_sample_count; ++pair_index) {
        std::uint32_t left_index = dist(rng);
        std::uint32_t right_index = dist(rng);
        while (right_index == left_index) {
            right_index = dist(rng);
        }

        const std::size_t overlap = OverlapCount(sorted_samples[left_index], sorted_samples[right_index]);
        const std::size_t union_count =
            sorted_samples[left_index].size() + sorted_samples[right_index].size() - overlap;
        overlaps.push_back(static_cast<std::uint32_t>(overlap));
        jaccards.push_back(union_count == 0U
            ? 0.0
            : static_cast<double>(overlap) / static_cast<double>(union_count));
    }

    std::vector<std::uint32_t> sorted_overlaps = overlaps;
    std::sort(sorted_overlaps.begin(), sorted_overlaps.end());
    std::vector<double> sorted_jaccards = jaccards;
    std::sort(sorted_jaccards.begin(), sorted_jaccards.end());

    const double overlap_mean = Mean(overlaps);
    const double jaccard_mean = jaccards.empty()
        ? 0.0
        : std::accumulate(jaccards.begin(), jaccards.end(), 0.0) / static_cast<double>(jaccards.size());

    return nlohmann::json{
        {"pairSampleCount", pair_sample_count},
        {"documentOverlapCount", {
            {"mean", overlap_mean},
            {"median", Median(sorted_overlaps)},
            {"min", sorted_overlaps.empty() ? 0U : sorted_overlaps.front()},
            {"max", sorted_overlaps.empty() ? 0U : sorted_overlaps.back()}
        }},
        {"jaccard", {
            {"mean", jaccard_mean},
            {"median", sorted_jaccards.empty() ? 0.0 : (sorted_jaccards.size() % 2U == 0U
                ? (sorted_jaccards[(sorted_jaccards.size() / 2U) - 1U] +
                   sorted_jaccards[sorted_jaccards.size() / 2U]) / 2.0
                : sorted_jaccards[sorted_jaccards.size() / 2U])},
            {"min", sorted_jaccards.empty() ? 0.0 : sorted_jaccards.front()},
            {"max", sorted_jaccards.empty() ? 0.0 : sorted_jaccards.back()}
        }},
        {"jaccardHistogram", BuildDoubleHistogram(jaccards, 0.005)}
    };
}

} // namespace

std::string SamplingDiagnosticsArtifactName(const std::string& sampling_design_id) {
    return std::string("sampling_diagnostics_") + sampling_design_id + ".json";
}

SamplingDiagnosticsBuilder::SamplingDiagnosticsBuilder() {}

nlohmann::json SamplingDiagnosticsBuilder::build(const SamplingDesign& design,
                                                 const std::vector<DocumentInfo>& pool,
                                                 const std::vector<SampledCorpus>& samples,
                                                 std::uint32_t pair_sample_count) const {
    std::unordered_map<DocId, std::size_t> doc_to_index;
    doc_to_index.reserve(pool.size());
    for (std::size_t i = 0; i < pool.size(); ++i) {
        doc_to_index[pool[i].document_id] = i;
    }

    std::vector<std::uint32_t> inclusion_counts(pool.size(), 0U);
    std::uint64_t total_document_selections = 0U;
    std::uint64_t total_tokens = 0U;
    std::uint64_t total_docs = 0U;
    std::uint32_t min_tokens = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t max_tokens = 0U;
    std::uint32_t min_docs = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t max_docs = 0U;

    for (std::size_t i = 0; i < samples.size(); ++i) {
        total_tokens += samples[i].token_count;
        total_docs += static_cast<std::uint64_t>(samples[i].document_ids.size());
        min_tokens = std::min(min_tokens, samples[i].token_count);
        max_tokens = std::max(max_tokens, samples[i].token_count);
        min_docs = std::min(min_docs, static_cast<std::uint32_t>(samples[i].document_ids.size()));
        max_docs = std::max(max_docs, static_cast<std::uint32_t>(samples[i].document_ids.size()));

        total_document_selections += static_cast<std::uint64_t>(samples[i].document_ids.size());
        for (std::size_t j = 0; j < samples[i].document_ids.size(); ++j) {
            const std::unordered_map<DocId, std::size_t>::const_iterator found =
                doc_to_index.find(samples[i].document_ids[j]);
            if (found != doc_to_index.end()) {
                ++inclusion_counts[found->second];
            }
        }
    }

    std::vector<std::uint32_t> sorted_counts = SortedCounts(inclusion_counts);
    const double mean_inclusion = Mean(inclusion_counts);
    const double stddev_inclusion = StandardDeviation(inclusion_counts, mean_inclusion);
    std::uint32_t unique_sampled = 0U;
    std::uint32_t never_sampled = 0U;
    for (std::size_t i = 0; i < inclusion_counts.size(); ++i) {
        if (inclusion_counts[i] == 0U) {
            ++never_sampled;
        } else {
            ++unique_sampled;
        }
    }

    const std::vector<InclusionRecord> records = BuildInclusionRecords(pool, inclusion_counts);

    return nlohmann::json{
        {"artifactType", "sampling_diagnostics"},
        {"samplingDesign", ToJson(design)},
        {"poolSummary", {
            {"eligibleDocumentCount", static_cast<std::uint32_t>(pool.size())},
            {"acceptedSampleCount", static_cast<std::uint32_t>(samples.size())},
            {"totalDocumentSelections", total_document_selections},
            {"uniqueDocumentsSampled", unique_sampled},
            {"neverSampledDocuments", never_sampled},
            {"sampledAtLeastOnceProportion", pool.empty() ? 0.0
                : static_cast<double>(unique_sampled) / static_cast<double>(pool.size())}
        }},
        {"sampleSummary", {
            {"meanSampleTokens", samples.empty() ? 0.0 : static_cast<double>(total_tokens) / static_cast<double>(samples.size())},
            {"minSampleTokens", samples.empty() ? 0U : min_tokens},
            {"maxSampleTokens", samples.empty() ? 0U : max_tokens},
            {"meanDocumentsPerSample", samples.empty() ? 0.0 : static_cast<double>(total_docs) / static_cast<double>(samples.size())},
            {"minDocumentsPerSample", samples.empty() ? 0U : min_docs},
            {"maxDocumentsPerSample", samples.empty() ? 0U : max_docs}
        }},
        {"documentInclusionSummary", {
            {"meanInclusionCount", mean_inclusion},
            {"medianInclusionCount", Median(sorted_counts)},
            {"minInclusionCount", sorted_counts.empty() ? 0U : sorted_counts.front()},
            {"maxInclusionCount", sorted_counts.empty() ? 0U : sorted_counts.back()},
            {"standardDeviation", stddev_inclusion},
            {"coefficientOfVariation", mean_inclusion <= 0.0 ? 0.0 : stddev_inclusion / mean_inclusion}
        }},
        {"documentInclusionHistogram", BuildIntegerHistogram(sorted_counts, 10U)},
        {"documentInclusionExtremes", ExtremesToJson(records, 20U)},
        {"lengthBiasSummary", BuildLengthBiasSummary(pool, inclusion_counts)},
        {"sampleOverlapSummary", BuildSampleOverlapSummary(samples, pair_sample_count, design.random_seed)}
    };
}

void SamplingDiagnosticsBuilder::write(const std::string& artifact_path,
                                       const nlohmann::json& payload) const {
    std::ofstream output(artifact_path.c_str(), std::ios::binary | std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Failed to open sampling diagnostics artifact for writing: " + artifact_path);
    }
    output << payload.dump(2) << std::endl;
}

} // namespace teknegram
