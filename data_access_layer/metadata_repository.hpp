#pragma once

#include <string>
#include <vector>

#include "../experiment_shared_layer/experiment_options.hpp"

struct sqlite3;

namespace teknegram {

class MetadataRepository {
    public:
        MetadataRepository();
        explicit MetadataRepository(const std::string& db_path);
        ~MetadataRepository();

        void open(const std::string& db_path);
        bool is_open() const;
        const std::string& db_path() const;

        std::vector<DocumentInfo> load_documents() const;
        void ensure_experiment_registry() const;
        std::string create_experiment_record(const ExperimentOptions& options,
                                             const std::string& artifact_dir,
                                             const std::string& created_at) const;
        void update_experiment_status(const std::string& experiment_code,
                                      const std::string& status,
                                      const std::string& completed_at) const;

    private:
        void close();

        sqlite3* db_;
        std::string db_path_;
};

} // namespace teknegram
