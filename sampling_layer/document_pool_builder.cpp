#include "document_pool_builder.hpp"

namespace teknegram {

DocumentPoolBuilder::DocumentPoolBuilder() {}

std::vector<DocumentInfo> DocumentPoolBuilder::build_pool(const CorpusDataset& dataset,
                                                          const CompositionConfig& composition_config) const {
    return build_pool(dataset.documents(), composition_config);
}

std::vector<DocumentInfo> DocumentPoolBuilder::build_pool(
    const std::vector<DocumentInfo>& documents,
    const CompositionConfig& composition_config) const {
    std::vector<DocumentInfo> pool;
    for (std::size_t i = 0; i < documents.size(); ++i) {
        if (matches_composition(documents[i], composition_config)) {
            DocumentInfo copy = documents[i];
            copy.metadata["composition_label"] = composition_config.label;
            pool.push_back(copy);
        }
    }
    return pool;
}

bool DocumentPoolBuilder::matches_composition(const DocumentInfo& document,
                                              const CompositionConfig& composition_config) const {
    if (composition_config.min_tokens > 0U && document.token_count < composition_config.min_tokens) {
        return false;
    }
    if (composition_config.max_tokens > 0U && document.token_count > composition_config.max_tokens) {
        return false;
    }
    return true;
}

} // namespace teknegram
