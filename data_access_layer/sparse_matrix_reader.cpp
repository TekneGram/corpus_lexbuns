#include "sparse_matrix_reader.hpp"

#include <fstream>
#include <stdexcept>

namespace teknegram {

namespace {

struct SparseMatrixMetaFile {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint64_t num_docs;
    std::uint64_t num_features;
    std::uint64_t num_nonzero;
    std::uint32_t entry_size_bytes;
    char reserved[32];
};

void ReadBytes(const std::string& path, char* data, std::streamsize size) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open binary file: " + path);
    }
    input.read(data, size);
    if (input.gcount() != size) {
        throw std::runtime_error("Failed to read expected number of bytes from: " + path);
    }
}

template <typename T>
std::vector<T> ReadVector(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("Failed to open binary file: " + path);
    }
    const std::ifstream::pos_type file_size = input.tellg();
    if (file_size < 0 || static_cast<std::uint64_t>(file_size) % sizeof(T) != 0) {
        throw std::runtime_error("Unexpected file size for: " + path);
    }
    std::vector<T> values(static_cast<std::size_t>(file_size) / sizeof(T));
    input.seekg(0, std::ios::beg);
    if (!values.empty()) {
        input.read(reinterpret_cast<char*>(&values[0]),
                   static_cast<std::streamsize>(values.size() * sizeof(T)));
    }
    return values;
}

} // namespace

SparseMatrixReader::SparseMatrixReader() {}

SparseMatrixReader::SparseMatrixReader(const std::string& family_prefix) {
    open(family_prefix);
}

void SparseMatrixReader::open(const std::string& family_prefix) {
    family_prefix_ = family_prefix;

    SparseMatrixMetaFile meta;
    ReadBytes(family_prefix + ".spm.meta.bin", reinterpret_cast<char*>(&meta), sizeof(meta));
    info_.num_docs = meta.num_docs;
    info_.num_features = meta.num_features;
    info_.num_nonzero = meta.num_nonzero;
    info_.entry_size_bytes = meta.entry_size_bytes;

    if (info_.entry_size_bytes != 8U) {
        throw std::runtime_error("Unsupported sparse entry size in " + family_prefix);
    }

    offsets_ = ReadVector<std::uint64_t>(family_prefix + ".spm.offsets.bin");
    if (offsets_.size() != info_.num_docs + 1U) {
        throw std::runtime_error("Sparse matrix offsets size mismatch for " + family_prefix);
    }

    entries_ = ReadVector<SparseEntry>(family_prefix + ".spm.entries.bin");
    if (entries_.size() != info_.num_nonzero) {
        throw std::runtime_error("Sparse matrix entry count mismatch for " + family_prefix);
    }
}

bool SparseMatrixReader::is_open() const {
    return !offsets_.empty() || !entries_.empty();
}

const SparseMatrixInfo& SparseMatrixReader::info() const {
    return info_;
}

std::size_t SparseMatrixReader::num_docs() const {
    return static_cast<std::size_t>(info_.num_docs);
}

std::size_t SparseMatrixReader::num_features() const {
    return static_cast<std::size_t>(info_.num_features);
}

void SparseMatrixReader::read_row(DocId document_id, std::vector<SparseEntry>* entries) const {
    if (!entries) {
        throw std::runtime_error("read_row requires a non-null output vector");
    }
    if (document_id >= offsets_.size() - 1U) {
        throw std::runtime_error("Document id out of range in sparse matrix");
    }
    const std::uint64_t begin = offsets_[document_id];
    const std::uint64_t end = offsets_[document_id + 1U];
    if (end < begin || end > entries_.size()) {
        throw std::runtime_error("Corrupt sparse matrix offsets");
    }
    entries->assign(entries_.begin() + static_cast<std::ptrdiff_t>(begin),
                    entries_.begin() + static_cast<std::ptrdiff_t>(end));
}

} // namespace teknegram
