#pragma once

#include <string>
#include <vector>

#include "../experiment_shared_layer/experiment_types.hpp"
#include "lexicon_reader.hpp"
#include "metadata_repository.hpp"
#include "sparse_matrix_reader.hpp"

namespace teknegram {

class CorpusDataset {
    public:
        CorpusDataset();
        explicit CorpusDataset(const std::string& dataset_dir);

        void open(const std::string& dataset_dir, std::uint32_t bundle_size);
        bool is_open() const;

        const std::string& dataset_dir() const;
        const std::string& experiments_dir() const;
        const std::vector<DocumentInfo>& documents() const;
        const SparseMatrixReader& sparse_matrix() const;
        const LexiconReader& word_lexicon() const;
        const LexiconReader& ngram_lexicon() const;
        MetadataRepository& metadata();
        const MetadataRepository& metadata() const;

        std::string resolve_artifact_path(const std::string& filename) const;

    private:
        void validate_required_files(const std::string& dataset_dir,
                                     std::uint32_t bundle_size) const;
        void attach_document_ranges();

        std::string dataset_dir_;
        std::string experiments_dir_;
        std::vector<DocumentInfo> documents_;
        SparseMatrixReader sparse_matrix_;
        LexiconReader word_lexicon_;
        LexiconReader ngram_lexicon_;
        MetadataRepository metadata_;
        bool is_open_;
};

} // namespace teknegram
