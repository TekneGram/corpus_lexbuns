#pragma once

#include <vector>

#include "../data_access_layer/corpus_dataset.hpp"

namespace teknegram {

class DocumentPoolBuilder {
    public:
        DocumentPoolBuilder();

        std::vector<DocumentInfo> build_pool(const CorpusDataset& dataset,
                                             const CompositionConfig& composition_config) const;
        std::vector<DocumentInfo> build_pool(const std::vector<DocumentInfo>& documents,
                                             const CompositionConfig& composition_config) const;
        bool matches_composition(const DocumentInfo& document,
                                 const CompositionConfig& composition_config) const;
};

} // namespace teknegram
