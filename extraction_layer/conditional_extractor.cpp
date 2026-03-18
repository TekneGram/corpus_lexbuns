#include "conditional_extractor.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>
#include <cstring>
#include <sys/stat.h>

#include "../experiment_shared_layer/json_io.hpp"

namespace teknegram {

class ProgressEmitter {
    public:
        virtual ~ProgressEmitter() {}
        virtual void emit(const std::string& message, int percent) const = 0;
};

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
        current.push_back(path[i]);
        if (path[i] == '/' || i + 1 == path.size()) {
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

void WriteUint32(std::ofstream* output, std::uint32_t value) {
    output->write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void WriteUint64(std::ofstream* output, std::uint64_t value) {
    output->write(reinterpret_cast<const char*>(&value), sizeof(value));
}

void WriteString(std::ofstream* output, const std::string& value) {
    const std::uint32_t size = static_cast<std::uint32_t>(value.size());
    WriteUint32(output, size);
    if (size > 0U) {
        output->write(value.data(), static_cast<std::streamsize>(size));
    }
}

std::string ReadString(std::ifstream* input) {
    std::uint32_t size = 0;
    input->read(reinterpret_cast<char*>(&size), sizeof(size));
    if (!(*input)) {
        throw std::runtime_error("Failed to read string length from aggregate artifact");
    }
    std::string value(size, '\0');
    if (size > 0U) {
        input->read(&value[0], static_cast<std::streamsize>(size));
        if (!(*input)) {
            throw std::runtime_error("Failed to read string from aggregate artifact");
        }
    }
    return value;
}

std::string AggregatePath(const std::string& root, const ExperimentCondition& condition) {
    return JoinPath(root, std::string("aggregate_") + condition.condition_id + ".bin");
}

std::string DebugPath(const std::string& root, const ExperimentCondition& condition) {
    return JoinPath(root, std::string("sample_sets_") + condition.condition_id + ".json");
}

void WriteAggregateBinary(const std::string& path, const ConditionAggregate& aggregate) {
    std::ofstream output(path.c_str(), std::ios::binary);
    if (!output) {
        throw std::runtime_error("Failed to open aggregate artifact for write: " + path);
    }

    const std::uint32_t magic = 0x4C424147U; // LBAG
    const std::uint32_t version = 1U;
    WriteUint32(&output, magic);
    WriteUint32(&output, version);
    WriteString(&output, aggregate.condition_id);
    WriteUint32(&output, aggregate.sample_count);
    WriteUint32(&output, aggregate.conditional_mode ? 1U : 0U);

    const std::uint64_t mass_size = static_cast<std::uint64_t>(aggregate.accumulated_mass.size());
    WriteUint64(&output, mass_size);
    for (std::size_t i = 0; i < aggregate.accumulated_mass.size(); ++i) {
        const double value = aggregate.accumulated_mass[i];
        output.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }

    const std::uint64_t count_size =
        static_cast<std::uint64_t>(aggregate.extracted_sample_counts.size());
    WriteUint64(&output, count_size);
    for (std::size_t i = 0; i < aggregate.extracted_sample_counts.size(); ++i) {
        WriteUint32(&output, aggregate.extracted_sample_counts[i]);
    }
}

ConditionAggregate ReadAggregateBinary(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open aggregate artifact for read: " + path);
    }

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    input.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    input.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (!input || magic != 0x4C424147U || version != 1U) {
        throw std::runtime_error("Invalid aggregate artifact: " + path);
    }

    ConditionAggregate aggregate;
    aggregate.condition_id = ReadString(&input);
    input.read(reinterpret_cast<char*>(&aggregate.sample_count), sizeof(aggregate.sample_count));
    std::uint32_t conditional_mode = 0;
    input.read(reinterpret_cast<char*>(&conditional_mode), sizeof(conditional_mode));
    aggregate.conditional_mode = conditional_mode != 0U;

    std::uint64_t mass_size = 0;
    input.read(reinterpret_cast<char*>(&mass_size), sizeof(mass_size));
    aggregate.accumulated_mass.resize(static_cast<std::size_t>(mass_size));
    for (std::size_t i = 0; i < aggregate.accumulated_mass.size(); ++i) {
        input.read(reinterpret_cast<char*>(&aggregate.accumulated_mass[i]),
                   sizeof(double));
    }

    std::uint64_t count_size = 0;
    input.read(reinterpret_cast<char*>(&count_size), sizeof(count_size));
    aggregate.extracted_sample_counts.resize(static_cast<std::size_t>(count_size));
    for (std::size_t i = 0; i < aggregate.extracted_sample_counts.size(); ++i) {
        input.read(reinterpret_cast<char*>(&aggregate.extracted_sample_counts[i]),
                   sizeof(std::uint32_t));
    }

    if (!input) {
        throw std::runtime_error("Failed to fully read aggregate artifact: " + path);
    }
    return aggregate;
}

} // namespace

ConditionalExtractor::ConditionalExtractor(const std::string& artifact_root,
                                           bool emit_debug_bundle_counts)
    : artifact_root_(artifact_root),
      emit_debug_bundle_counts_(emit_debug_bundle_counts) {}

ConditionAggregate ConditionalExtractor::run(const CorpusDataset& dataset,
                                             const ExperimentCondition& condition,
                                             const std::vector<SampledCorpus>& samples,
                                             const ProgressEmitter* progress_emitter) const {
    (void)dataset;
    ThresholdPolicy policy;
    ConditionAggregate aggregate;
    aggregate.condition_id = condition.condition_id;
    aggregate.sample_count = static_cast<std::uint32_t>(samples.size());
    aggregate.conditional_mode = true;

    BundleCounter counter(artifact_root_, emit_debug_bundle_counts_);
    std::vector<SampleExtractionResult> sample_results;
    sample_results.reserve(samples.size());

    for (std::size_t i = 0; i < samples.size(); ++i) {
        if (progress_emitter && (i == 0 || i % std::max<std::size_t>(1, samples.size() / 10U) == 0U)) {
            progress_emitter->emit("Conditional extraction processing sample " +
                                       std::to_string(i + 1) + "/" + std::to_string(samples.size()),
                                   static_cast<int>((100U * i) / std::max<std::size_t>(1, samples.size())));
        }
        const SampleBundleStats stats = counter.count_sample(dataset, samples[i], condition.bundle_size);
        SampleExtractionResult extracted = extract_sample(stats, condition, policy);
        update_aggregate(stats, extracted, &aggregate);
        sample_results.push_back(extracted);
        if (emit_debug_bundle_counts_) {
            counter.write_debug_bundle_counts(condition, samples[i], stats);
        }
    }

    write_aggregate_artifact(condition, aggregate);
    if (emit_debug_bundle_counts_) {
        write_sample_level_sets_debug(condition, sample_results);
    }
    if (progress_emitter) {
        progress_emitter->emit("Conditional extraction complete for " + condition.condition_id, 100);
    }
    return aggregate;
}

SampleExtractionResult ConditionalExtractor::extract_sample(const SampleBundleStats& stats,
                                                            const ExperimentCondition& condition,
                                                            const ThresholdPolicy& policy) const {
    SampleExtractionResult result;
    result.sample_index = stats.sample_index;
    for (std::size_t feature_id = 0; feature_id < stats.feature_counts.size(); ++feature_id) {
        const std::uint32_t raw_count = stats.feature_counts[feature_id];
        const std::uint16_t document_range =
            feature_id < stats.document_ranges.size()
                ? static_cast<std::uint16_t>(stats.document_ranges[feature_id])
                : 0U;
        if (policy.passes_conditional_rule(raw_count, document_range, condition)) {
            result.selected_feature_ids.push_back(static_cast<FeatureId>(feature_id));
            result.selected_raw_counts.push_back(raw_count);
            result.selected_document_ranges.push_back(document_range);
        }
    }
    return result;
}

void ConditionalExtractor::update_aggregate(const SampleBundleStats& stats,
                                            const SampleExtractionResult& extracted,
                                            ConditionAggregate* aggregate) const {
    if (!aggregate) {
        throw std::runtime_error("update_aggregate requires a non-null aggregate");
    }
    if (aggregate->accumulated_mass.size() < stats.feature_counts.size()) {
        aggregate->accumulated_mass.resize(stats.feature_counts.size(), 0.0);
    }
    if (aggregate->extracted_sample_counts.size() < stats.feature_counts.size()) {
        aggregate->extracted_sample_counts.resize(stats.feature_counts.size(), 0U);
    }

    for (std::size_t i = 0; i < extracted.selected_feature_ids.size(); ++i) {
        const FeatureId feature_id = extracted.selected_feature_ids[i];
        if (feature_id >= aggregate->accumulated_mass.size()) {
            continue;
        }
        aggregate->accumulated_mass[feature_id] += extracted.selected_raw_counts[i];
        ++aggregate->extracted_sample_counts[feature_id];
    }
}

void ConditionalExtractor::write_aggregate_artifact(const ExperimentCondition& condition,
                                                    const ConditionAggregate& aggregate) const {
    EnsureDirectoryExists(artifact_root_);
    const std::string path = AggregatePath(artifact_root_, condition);
    WriteAggregateBinary(path, aggregate);
}

ConditionAggregate ConditionalExtractor::load_aggregate_artifact(const std::string& artifact_path) const {
    return ReadAggregateBinary(artifact_path);
}

void ConditionalExtractor::write_sample_level_sets_debug(
    const ExperimentCondition& condition,
    const std::vector<SampleExtractionResult>& sample_results) const {
    if (!emit_debug_bundle_counts_) {
        return;
    }
    EnsureDirectoryExists(artifact_root_);
    const std::string path = DebugPath(artifact_root_, condition);
    std::ofstream output(path.c_str(), std::ios::binary);
    if (!output) {
        throw std::runtime_error("Failed to open sample set debug file: " + path);
    }
    nlohmann::json payload = nlohmann::json::array();
    for (std::size_t i = 0; i < sample_results.size(); ++i) {
        payload.push_back(nlohmann::json{
            {"sampleIndex", sample_results[i].sample_index},
            {"featureIds", sample_results[i].selected_feature_ids},
            {"rawCounts", sample_results[i].selected_raw_counts},
            {"documentRanges", sample_results[i].selected_document_ranges}
        });
    }
    const std::string text = payload.dump(2);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
}

} // namespace teknegram
