#include "corpus_dataset.hpp"

#include <fstream>
#include <stdexcept>

namespace teknegram {

namespace {

bool FileExists(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary);
    return static_cast<bool>(input);
}

std::string JoinPath(const std::string& left, const std::string& right) {
    if (left.empty()) {
        return right;
    }
    if (left[left.size() - 1] == '/') {
        return left + right;
    }
    return left + "/" + right;
}

struct DocumentRange {
    std::uint32_t start;
    std::uint32_t end;
};

std::vector<DocumentRange> ReadDocRanges(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("Failed to open document ranges file: " + path);
    }

    const std::ifstream::pos_type file_size = input.tellg();
    if (file_size < 0 || static_cast<std::uint64_t>(file_size) % sizeof(DocumentRange) != 0) {
        throw std::runtime_error("Unexpected document ranges file size: " + path);
    }

    std::vector<DocumentRange> ranges(static_cast<std::size_t>(file_size) / sizeof(DocumentRange));
    input.seekg(0, std::ios::beg);
    if (!ranges.empty()) {
        input.read(reinterpret_cast<char*>(&ranges[0]),
                   static_cast<std::streamsize>(ranges.size() * sizeof(DocumentRange)));
    }
    return ranges;
}

} // namespace

CorpusDataset::CorpusDataset()
    : is_open_(false) {}

CorpusDataset::CorpusDataset(const std::string& dataset_dir)
    : is_open_(false) {
    open(dataset_dir, 4);
}

void CorpusDataset::open(const std::string& dataset_dir, std::uint32_t bundle_size) {
    validate_required_files(dataset_dir, bundle_size);

    dataset_dir_ = dataset_dir;
    experiments_dir_ = JoinPath(dataset_dir, "experiments");
    metadata_.open(JoinPath(dataset_dir, "corpus.db"));
    documents_ = metadata_.load_documents();
    attach_document_ranges();
    sparse_matrix_.open(JoinPath(dataset_dir, std::to_string(bundle_size) + "gram"));
    word_lexicon_.open(JoinPath(dataset_dir, "word.lexicon.bin"));
    ngram_lexicon_.open(JoinPath(dataset_dir, std::to_string(bundle_size) + "gram.lexicon.bin"));

    if (documents_.size() != sparse_matrix_.num_docs()) {
        throw std::runtime_error("Document count mismatch between metadata and sparse matrix");
    }

    is_open_ = true;
}

bool CorpusDataset::is_open() const {
    return is_open_;
}

const std::string& CorpusDataset::dataset_dir() const {
    return dataset_dir_;
}

const std::string& CorpusDataset::experiments_dir() const {
    return experiments_dir_;
}

const std::vector<DocumentInfo>& CorpusDataset::documents() const {
    return documents_;
}

const SparseMatrixReader& CorpusDataset::sparse_matrix() const {
    return sparse_matrix_;
}

const LexiconReader& CorpusDataset::word_lexicon() const {
    return word_lexicon_;
}

const LexiconReader& CorpusDataset::ngram_lexicon() const {
    return ngram_lexicon_;
}

MetadataRepository& CorpusDataset::metadata() {
    return metadata_;
}

const MetadataRepository& CorpusDataset::metadata() const {
    return metadata_;
}

std::string CorpusDataset::resolve_artifact_path(const std::string& filename) const {
    return JoinPath(experiments_dir_, filename);
}

void CorpusDataset::validate_required_files(const std::string& dataset_dir,
                                            std::uint32_t bundle_size) const {
    const std::string bundle_prefix = JoinPath(dataset_dir, std::to_string(bundle_size) + "gram");
    const char* required_paths[] = {
        "corpus.db",
        "doc_ranges.bin",
        "word.lexicon.bin"
    };

    for (std::size_t i = 0; i < sizeof(required_paths) / sizeof(required_paths[0]); ++i) {
        const std::string path = JoinPath(dataset_dir, required_paths[i]);
        if (!FileExists(path)) {
            throw std::runtime_error("Missing required dataset file: " + path);
        }
    }

    const char* sparse_suffixes[] = {
        ".spm.meta.bin",
        ".spm.offsets.bin",
        ".spm.entries.bin",
        ".lexicon.bin"
    };
    for (std::size_t i = 0; i < sizeof(sparse_suffixes) / sizeof(sparse_suffixes[0]); ++i) {
        const std::string path = bundle_prefix + sparse_suffixes[i];
        if (!FileExists(path)) {
            throw std::runtime_error("Missing required bundle dataset file: " + path);
        }
    }
}

void CorpusDataset::attach_document_ranges() {
    const std::vector<DocumentRange> ranges = ReadDocRanges(JoinPath(dataset_dir_, "doc_ranges.bin"));
    if (ranges.size() != documents_.size()) {
        throw std::runtime_error("Document range count mismatch");
    }

    for (std::size_t i = 0; i < documents_.size(); ++i) {
        documents_[i].token_start = ranges[i].start;
        documents_[i].token_end = ranges[i].end;
        documents_[i].token_count = (ranges[i].end >= ranges[i].start)
            ? (ranges[i].end - ranges[i].start + 1U)
            : 0U;
    }
}

} // namespace teknegram
