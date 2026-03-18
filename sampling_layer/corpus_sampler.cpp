#include "corpus_sampler.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <random>
#include <numeric>
#include <stdexcept>

#include <sys/stat.h>

#include "../experiment_shared_layer/json_io.hpp"

namespace teknegram {

namespace {

struct MembershipHeader {
    char magic[8];
    std::uint32_t version;
    std::uint32_t random_seed;
    std::uint32_t target_sample_count;
    std::uint32_t accepted_sample_count;
    std::uint32_t corpus_size_tokens;
    std::uint32_t corpus_delta_tokens;
    std::uint32_t sampling_design_id_length;
    std::uint32_t composition_label_length;
};

struct SampleHeader {
    std::uint32_t sample_index;
    std::uint32_t token_count;
    std::uint32_t document_count;
};

void EnsureParentDirectory(const std::string& path) {
    const std::string::size_type slash = path.find_last_of('/');
    if (slash == std::string::npos) {
        return;
    }
    const std::string dir = path.substr(0, slash);
    if (dir.empty()) {
        return;
    }

    std::string current;
    for (std::size_t i = 0; i < dir.size(); ++i) {
        current.push_back(dir[i]);
        if (dir[i] == '/') {
            if (!current.empty()) {
                ::mkdir(current.c_str(), 0755);
            }
        }
    }
    ::mkdir(dir.c_str(), 0755);
}

std::uint32_t ComputeDistance(std::uint32_t lhs, std::uint32_t rhs) {
    return (lhs > rhs) ? (lhs - rhs) : (rhs - lhs);
}

std::uint32_t TargetLowerBound(const SamplingDesign& design) {
    return (design.corpus_size_tokens > design.corpus_delta_tokens)
        ? (design.corpus_size_tokens - design.corpus_delta_tokens)
        : 0U;
}

std::uint32_t TargetUpperBound(const SamplingDesign& design) {
    const std::uint64_t sum =
        static_cast<std::uint64_t>(design.corpus_size_tokens) +
        static_cast<std::uint64_t>(design.corpus_delta_tokens);
    return (sum > std::numeric_limits<std::uint32_t>::max())
        ? std::numeric_limits<std::uint32_t>::max()
        : static_cast<std::uint32_t>(sum);
}

struct AttemptResult {
    bool accepted;
    std::uint32_t distance;
    SampledCorpus sample;

    AttemptResult()
        : accepted(false),
          distance(std::numeric_limits<std::uint32_t>::max()) {}
};

AttemptResult AttemptBuildSample(const SamplingDesign& design,
                                const std::vector<DocumentInfo>& pool,
                                std::uint32_t seed,
                                std::uint32_t sample_index) {
    AttemptResult result;
    if (pool.empty()) {
        return result;
    }

    std::vector<std::size_t> order(pool.size());
    std::iota(order.begin(), order.end(), 0U);
    std::mt19937 rng(seed);
    std::shuffle(order.begin(), order.end(), rng);

    const std::uint32_t lower = TargetLowerBound(design);
    const std::uint32_t upper = TargetUpperBound(design);

    SampledCorpus current;
    current.sample_index = sample_index;
    current.token_count = 0U;

    std::vector<DocId> best_document_ids;
    std::uint32_t best_token_count = 0U;
    std::uint32_t best_distance = std::numeric_limits<std::uint32_t>::max();

    for (std::size_t i = 0; i < order.size(); ++i) {
        const DocumentInfo& document = pool[order[i]];
        current.document_ids.push_back(document.document_id);
        current.token_count += document.token_count;

        const std::uint32_t distance = ComputeDistance(current.token_count, design.corpus_size_tokens);
        if (distance < best_distance) {
            best_distance = distance;
            best_document_ids = current.document_ids;
            best_token_count = current.token_count;
        }

        if (current.token_count >= lower && current.token_count <= upper) {
            result.accepted = true;
            result.distance = distance;
            result.sample = current;
            return result;
        }

        if (current.token_count > upper) {
            break;
        }
    }

    result.distance = best_distance;
    result.sample.sample_index = sample_index;
    result.sample.token_count = best_token_count;
    result.sample.document_ids = best_document_ids;
    return result;
}

SampleSummary BuildSummaryFromSamples(const SamplingDesign& design,
                                     const std::vector<SampledCorpus>& samples,
                                     const std::string& membership_artifact_path,
                                     const std::string& run_manifest_path) {
    SampleSummary summary;
    summary.sampling_design_id = design.sampling_design_id;
    summary.corpus_size_tokens = design.corpus_size_tokens;
    summary.corpus_delta_tokens = design.corpus_delta_tokens;
    summary.composition_label = design.composition_label;
    summary.random_seed = design.random_seed;
    summary.target_sample_count = design.sample_count;
    summary.accepted_sample_count = static_cast<std::uint32_t>(samples.size());
    summary.sample_membership_artifact_path = membership_artifact_path;
    summary.run_manifest_path = run_manifest_path;

    if (samples.empty()) {
        return summary;
    }

    std::uint64_t total_tokens = 0U;
    std::uint64_t total_docs = 0U;
    std::uint32_t min_tokens = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t max_tokens = 0U;
    std::uint32_t min_docs = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t max_docs = 0U;

    for (std::size_t i = 0; i < samples.size(); ++i) {
        const SampledCorpus& sample = samples[i];
        total_tokens += sample.token_count;
        total_docs += static_cast<std::uint64_t>(sample.document_ids.size());
        if (sample.token_count < min_tokens) {
            min_tokens = sample.token_count;
        }
        if (sample.token_count > max_tokens) {
            max_tokens = sample.token_count;
        }
        if (sample.document_ids.size() < min_docs) {
            min_docs = static_cast<std::uint32_t>(sample.document_ids.size());
        }
        if (sample.document_ids.size() > max_docs) {
            max_docs = static_cast<std::uint32_t>(sample.document_ids.size());
        }
    }

    summary.mean_sample_tokens = static_cast<double>(total_tokens) / static_cast<double>(samples.size());
    summary.min_sample_tokens = min_tokens;
    summary.max_sample_tokens = max_tokens;
    summary.mean_documents_per_sample = static_cast<double>(total_docs) / static_cast<double>(samples.size());
    summary.min_documents_per_sample = min_docs;
    summary.max_documents_per_sample = max_docs;
    return summary;
}

} // namespace

std::string SampleMembershipArtifactName(const std::string& sampling_design_id) {
    return std::string("sample_membership_") + sampling_design_id + ".bin";
}

std::string SampleSummaryArtifactName(const std::string& sampling_design_id) {
    return std::string("sample_summary_") + sampling_design_id + ".json";
}

CorpusSampler::CorpusSampler()
    : random_seed_(0) {}

CorpusSampler::CorpusSampler(std::uint32_t random_seed)
    : random_seed_(random_seed) {}

std::vector<SampledCorpus> CorpusSampler::build_samples(const SamplingDesign& design,
                                                       const std::vector<DocumentInfo>& pool) const {
    std::vector<SampledCorpus> samples;
    samples.reserve(design.sample_count);

    for (std::uint32_t sample_index = 0; sample_index < design.sample_count; ++sample_index) {
        AttemptResult best_attempt;
        for (std::uint32_t attempt = 0; attempt < 5U; ++attempt) {
            const std::uint32_t seed = random_seed_
                ^ design.random_seed
                ^ (sample_index * 2654435761U)
                ^ (attempt * 2246822519U);
            AttemptResult attempt_result = AttemptBuildSample(design, pool, seed, sample_index);
            if (attempt_result.accepted) {
                samples.push_back(attempt_result.sample);
                best_attempt = AttemptResult();
                break;
            }
            if (attempt_result.distance < best_attempt.distance) {
                best_attempt = attempt_result;
            }
            if (attempt == 4U) {
                samples.push_back(best_attempt.sample);
            }
        }
    }

    return samples;
}

std::vector<SampledCorpus> CorpusSampler::build_samples(const CorpusDataset& dataset,
                                                       const SamplingDesign& design,
                                                       const std::vector<DocumentInfo>& pool) const {
    (void)dataset;
    return build_samples(design, pool);
}

void CorpusSampler::write_membership_artifact(const std::string& path,
                                             const SamplingDesign& design,
                                             const std::vector<SampledCorpus>& samples) const {
    EnsureParentDirectory(path);

    std::ofstream out(path.c_str(), std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open sample membership artifact for writing: " + path);
    }

    MembershipHeader header;
    header.magic[0] = 'S';
    header.magic[1] = 'M';
    header.magic[2] = 'P';
    header.magic[3] = 'M';
    header.magic[4] = 'E';
    header.magic[5] = 'M';
    header.magic[6] = '1';
    header.magic[7] = '\0';
    header.version = 1U;
    header.random_seed = design.random_seed;
    header.target_sample_count = design.sample_count;
    header.accepted_sample_count = static_cast<std::uint32_t>(samples.size());
    header.corpus_size_tokens = design.corpus_size_tokens;
    header.corpus_delta_tokens = design.corpus_delta_tokens;
    header.sampling_design_id_length = static_cast<std::uint32_t>(design.sampling_design_id.size());
    header.composition_label_length = static_cast<std::uint32_t>(design.composition_label.size());

    out.write(reinterpret_cast<const char*>(&header), sizeof(header));
    if (!out) {
        throw std::runtime_error("Failed to write sample membership header");
    }
    out.write(design.sampling_design_id.data(),
              static_cast<std::streamsize>(design.sampling_design_id.size()));
    out.write(design.composition_label.data(),
              static_cast<std::streamsize>(design.composition_label.size()));
    if (!out) {
        throw std::runtime_error("Failed to write sample membership metadata");
    }

    for (std::size_t i = 0; i < samples.size(); ++i) {
        const SampledCorpus& sample = samples[i];
        SampleHeader sample_header;
        sample_header.sample_index = sample.sample_index;
        sample_header.token_count = sample.token_count;
        sample_header.document_count = static_cast<std::uint32_t>(sample.document_ids.size());
        out.write(reinterpret_cast<const char*>(&sample_header), sizeof(sample_header));
        if (!out) {
            throw std::runtime_error("Failed to write sample header");
        }
        if (!sample.document_ids.empty()) {
            out.write(reinterpret_cast<const char*>(&sample.document_ids[0]),
                      static_cast<std::streamsize>(sample.document_ids.size() * sizeof(DocId)));
            if (!out) {
                throw std::runtime_error("Failed to write sample document ids");
            }
        }
    }
}

std::vector<SampledCorpus> CorpusSampler::load_membership_artifact(const std::string& path,
                                                                  SamplingDesign* design_out) const {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open sample membership artifact: " + path);
    }

    MembershipHeader header;
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!in) {
        throw std::runtime_error("Failed to read sample membership header");
    }
    if (header.magic[0] != 'S' || header.magic[1] != 'M' || header.magic[2] != 'P' || header.magic[3] != 'M') {
        throw std::runtime_error("Invalid sample membership artifact magic");
    }

    std::string design_id(header.sampling_design_id_length, '\0');
    std::string composition_label(header.composition_label_length, '\0');
    if (header.sampling_design_id_length > 0U) {
        in.read(&design_id[0], static_cast<std::streamsize>(header.sampling_design_id_length));
    }
    if (header.composition_label_length > 0U) {
        in.read(&composition_label[0], static_cast<std::streamsize>(header.composition_label_length));
    }
    if (!in) {
        throw std::runtime_error("Failed to read sample membership metadata");
    }

    if (design_out) {
        design_out->sampling_design_id = design_id;
        design_out->corpus_size_tokens = header.corpus_size_tokens;
        design_out->corpus_delta_tokens = header.corpus_delta_tokens;
        design_out->composition_label = composition_label;
        design_out->sample_count = header.target_sample_count;
        design_out->random_seed = header.random_seed;
    }

    std::vector<SampledCorpus> samples;
    samples.reserve(header.accepted_sample_count);
    for (std::uint32_t i = 0; i < header.accepted_sample_count; ++i) {
        SampleHeader sample_header;
        in.read(reinterpret_cast<char*>(&sample_header), sizeof(sample_header));
        if (!in) {
            throw std::runtime_error("Failed to read sample header");
        }
        SampledCorpus sample;
        sample.sample_index = sample_header.sample_index;
        sample.token_count = sample_header.token_count;
        sample.document_ids.resize(sample_header.document_count);
        if (sample_header.document_count > 0U) {
            in.read(reinterpret_cast<char*>(&sample.document_ids[0]),
                    static_cast<std::streamsize>(sample_header.document_count * sizeof(DocId)));
            if (!in) {
                throw std::runtime_error("Failed to read sample document ids");
            }
        }
        samples.push_back(sample);
    }

    return samples;
}

void CorpusSampler::write_summary_json(const std::string& path, const SampleSummary& summary) const {
    EnsureParentDirectory(path);
    std::ofstream out(path.c_str(), std::ios::binary);
    if (!out) {
        throw std::runtime_error("Failed to open sample summary file for writing: " + path);
    }
    out << ToJson(summary).dump(2);
    if (!out) {
        throw std::runtime_error("Failed to write sample summary json");
    }
}

SampleSummary CorpusSampler::load_summary_json(const std::string& path) const {
    std::ifstream in(path.c_str(), std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open sample summary file: " + path);
    }
    const std::string input((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const nlohmann::json json = nlohmann::json::parse(input);
    return SampleSummaryFromJson(json);
}

SampleSummary CorpusSampler::build_summary(const SamplingDesign& design,
                                          const std::vector<SampledCorpus>& samples,
                                          const std::string& membership_artifact_path,
                                          const std::string& run_manifest_path) const {
    return BuildSummaryFromSamples(design, samples, membership_artifact_path, run_manifest_path);
}

std::vector<SampledCorpus> CorpusSampler::resume_or_sample(const std::string& membership_artifact_path,
                                                          const SamplingDesign& design,
                                                          const std::vector<DocumentInfo>& pool,
                                                          bool* loaded_existing) const {
    std::ifstream existing(membership_artifact_path.c_str(), std::ios::binary);
    if (existing) {
        if (loaded_existing) {
            *loaded_existing = true;
        }
        return load_membership_artifact(membership_artifact_path, 0);
    }
    if (loaded_existing) {
        *loaded_existing = false;
    }
    const std::vector<SampledCorpus> samples = build_samples(design, pool);
    write_membership_artifact(membership_artifact_path, design, samples);
    return samples;
}

} // namespace teknegram
