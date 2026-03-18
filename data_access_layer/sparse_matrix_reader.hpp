#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "../experiment_shared_layer/experiment_types.hpp"

namespace teknegram {

struct SparseEntry {
    FeatureId feature_id;
    std::uint32_t count;

    SparseEntry()
        : feature_id(0),
          count(0) {}
};

struct SparseMatrixInfo {
    std::uint64_t num_docs;
    std::uint64_t num_features;
    std::uint64_t num_nonzero;
    std::uint32_t entry_size_bytes;

    SparseMatrixInfo()
        : num_docs(0),
          num_features(0),
          num_nonzero(0),
          entry_size_bytes(0) {}
};

class SparseMatrixReader {
    public:
        SparseMatrixReader();
        explicit SparseMatrixReader(const std::string& family_prefix);

        void open(const std::string& family_prefix);

        bool is_open() const;
        const SparseMatrixInfo& info() const;
        std::size_t num_docs() const;
        std::size_t num_features() const;
        void read_row(DocId document_id, std::vector<SparseEntry>* entries) const;

    private:
        std::string family_prefix_;
        SparseMatrixInfo info_;
        std::vector<std::uint64_t> offsets_;
        std::vector<SparseEntry> entries_;
};

} // namespace teknegram
