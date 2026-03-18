#include "bundle_counter.hpp"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/types.h>

#include "../experiment_shared_layer/json_io.hpp"

namespace teknegram {

namespace {

std::string JoinPath(const std::string& left, const std::string& right) {
    if (left.empty()) {
        return right;
    }
    if (!left.empty() && left[left.size() - 1] == '/') {
        return left + right;
    }
    return left + "/" + right;
}

void EnsureDirectoryExists(const std::string& path) {
    if (path.empty()) {
        return;
    }

    std::string current;
    for (std::size_t i = 0; i < path.size(); ++i) {
        const char ch = path[i];
        current.push_back(ch);
        if (ch == '/' || i + 1 == path.size()) {
            if (current.size() == 1 && current[0] == '/') {
                continue;
            }
            if (current[current.size() - 1] == '/') {
                current.erase(current.size() - 1);
            }
            if (current.empty()) {
                continue;
            }
            struct stat stat_info;
            if (stat(current.c_str(), &stat_info) != 0) {
                if (mkdir(current.c_str(), 0755) != 0) {
                    throw std::runtime_error("Failed to create directory: " + current);
                }
            } else if (!S_ISDIR(stat_info.st_mode)) {
                throw std::runtime_error("Path exists and is not a directory: " + current);
            }
        }
    }
}

std::string MakeDebugPath(const std::string& root,
                          const ExperimentCondition& condition,
                          std::uint32_t sample_index) {
    return JoinPath(root, std::string("debug_bundle_counts_") + condition.condition_id +
                           "_sample_" + std::to_string(sample_index) + ".json");
}

} // namespace

BundleCounter::BundleCounter(const std::string& artifact_root,
                             bool emit_debug_bundle_counts)
    : artifact_root_(artifact_root),
      emit_debug_bundle_counts_(emit_debug_bundle_counts) {}

SampleBundleStats BundleCounter::count_sample(const CorpusDataset& dataset,
                                              const SampledCorpus& sample,
                                              std::uint32_t bundle_size) const {
    SampleBundleStats stats;
    stats.sample_index = sample.sample_index;
    std::vector<std::uint16_t> document_ranges_16;
    accumulate_sample(dataset, sample, &stats.feature_counts, &document_ranges_16);
    stats.document_ranges.assign(document_ranges_16.begin(), document_ranges_16.end());

    if (bundle_size == 0U) {
        throw std::runtime_error("bundle_size must be greater than zero");
    }
    return stats;
}

void BundleCounter::accumulate_sample(const CorpusDataset& dataset,
                                     const SampledCorpus& sample,
                                     std::vector<std::uint32_t>* feature_counts,
                                     std::vector<std::uint16_t>* feature_doc_ranges) const {
    if (!feature_counts || !feature_doc_ranges) {
        throw std::runtime_error("accumulate_sample requires non-null output vectors");
    }

    const std::size_t feature_count = dataset.sparse_matrix().num_features();
    feature_counts->assign(feature_count, 0U);
    feature_doc_ranges->assign(feature_count, 0U);

    std::vector<SparseEntry> row;
    row.reserve(256);

    for (std::size_t i = 0; i < sample.document_ids.size(); ++i) {
        const DocId document_id = sample.document_ids[i];
        row.clear();
        dataset.sparse_matrix().read_row(document_id, &row);
        for (std::size_t j = 0; j < row.size(); ++j) {
            const FeatureId feature_id = row[j].feature_id;
            if (feature_id >= feature_counts->size()) {
                throw std::runtime_error("Feature id out of range while counting sample");
            }
            (*feature_counts)[feature_id] += row[j].count;
            if ((*feature_doc_ranges)[feature_id] != std::numeric_limits<std::uint16_t>::max()) {
                ++(*feature_doc_ranges)[feature_id];
            }
        }
    }
}

void BundleCounter::write_debug_bundle_counts(const ExperimentCondition& condition,
                                              const SampledCorpus& sample,
                                              const SampleBundleStats& stats) const {
    if (!emit_debug_bundle_counts_) {
        return;
    }
    EnsureDirectoryExists(artifact_root_);
    const std::string path = MakeDebugPath(artifact_root_, condition, sample.sample_index);
    std::ofstream output(path.c_str(), std::ios::binary);
    if (!output) {
        throw std::runtime_error("Failed to open debug bundle-count file: " + path);
    }

    nlohmann::json payload = nlohmann::json{
        {"conditionId", condition.condition_id},
        {"sampleIndex", sample.sample_index},
        {"featureCounts", stats.feature_counts},
        {"documentRanges", stats.document_ranges}
    };
    const std::string text = payload.dump(2);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

} // namespace teknegram
