#include "metadata_repository.hpp"

#include <sqlite3.h>

#include <stdexcept>

#include "../experiment_shared_layer/json_io.hpp"

namespace teknegram {

namespace {

void ExecuteSql(sqlite3* db, const char* sql, const char* context) {
    char* err = 0;
    if (sqlite3_exec(db, sql, 0, 0, &err) != SQLITE_OK) {
        const std::string message = err ? err : "unknown sqlite error";
        sqlite3_free(err);
        throw std::runtime_error(std::string(context) + ": " + message);
    }
}

void BindText(sqlite3_stmt* stmt, int index, const std::string& value) {
    if (sqlite3_bind_text(stmt, index, value.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        throw std::runtime_error("Failed to bind sqlite text value");
    }
}

std::string ColumnText(sqlite3_stmt* stmt, int index) {
    const unsigned char* value = sqlite3_column_text(stmt, index);
    return value ? reinterpret_cast<const char*>(value) : "";
}

} // namespace

MetadataRepository::MetadataRepository()
    : db_(0) {}

MetadataRepository::MetadataRepository(const std::string& db_path)
    : db_(0) {
    open(db_path);
}

MetadataRepository::~MetadataRepository() {
    close();
}

void MetadataRepository::open(const std::string& db_path) {
    close();
    if (sqlite3_open(db_path.c_str(), &db_) != SQLITE_OK) {
        const std::string err = db_ ? sqlite3_errmsg(db_) : "unknown sqlite open error";
        close();
        throw std::runtime_error("Failed to open metadata DB: " + err);
    }
    db_path_ = db_path;
}

bool MetadataRepository::is_open() const {
    return db_ != 0;
}

const std::string& MetadataRepository::db_path() const {
    return db_path_;
}

std::vector<DocumentInfo> MetadataRepository::load_documents() const {
    if (!db_) {
        throw std::runtime_error("Metadata repository is not open");
    }

    const char* sql =
        "SELECT document_id, title, author, relative_path "
        "FROM document ORDER BY document_id;";

    sqlite3_stmt* stmt = 0;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        throw std::runtime_error(std::string("Failed to prepare document query: ") + sqlite3_errmsg(db_));
    }

    std::vector<DocumentInfo> documents;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        DocumentInfo info;
        info.document_id = static_cast<DocId>(sqlite3_column_int(stmt, 0));
        info.title = ColumnText(stmt, 1);
        info.author = ColumnText(stmt, 2);
        info.relative_path = ColumnText(stmt, 3);
        documents.push_back(info);
    }

    sqlite3_finalize(stmt);

    const char* group_sql =
        "SELECT dg.document_id, sk.key_name, sv.value_text "
        "FROM document_group dg "
        "JOIN semantic_key sk ON sk.key_id = dg.key_id "
        "JOIN semantic_value sv ON sv.value_id = dg.value_id "
        "ORDER BY dg.document_id;";

    if (sqlite3_prepare_v2(db_, group_sql, -1, &stmt, 0) != SQLITE_OK) {
        throw std::runtime_error(std::string("Failed to prepare document_group query: ")
                                 + sqlite3_errmsg(db_));
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const DocId document_id = static_cast<DocId>(sqlite3_column_int(stmt, 0));
        const std::string key_name = ColumnText(stmt, 1);
        const std::string value_text = ColumnText(stmt, 2);
        if (document_id < documents.size()) {
            documents[document_id].metadata[key_name] = value_text;
        }
    }
    sqlite3_finalize(stmt);
    return documents;
}

void MetadataRepository::ensure_experiment_registry() const {
    if (!db_) {
        throw std::runtime_error("Metadata repository is not open");
    }

    const char* sql =
        "CREATE TABLE IF NOT EXISTS experiments ("
        "experiment_id INTEGER PRIMARY KEY,"
        "experiment_code TEXT NOT NULL UNIQUE,"
        "title TEXT NOT NULL,"
        "artifact_dir TEXT NOT NULL,"
        "mode TEXT NOT NULL,"
        "status TEXT NOT NULL,"
        "created_at TEXT NOT NULL,"
        "completed_at TEXT,"
        "random_seed INTEGER,"
        "options_json TEXT NOT NULL"
        ");";
    ExecuteSql(db_, sql, "Failed to ensure experiments schema");
}

std::string MetadataRepository::create_experiment_record(const ExperimentOptions& options,
                                                         const std::string& artifact_dir,
                                                         const std::string& created_at) const {
    ensure_experiment_registry();

    std::string experiment_code = options.requested_experiment_code;
    if (experiment_code.empty()) {
        sqlite3_stmt* stmt = 0;
        if (sqlite3_prepare_v2(db_, "SELECT COALESCE(MAX(experiment_id), 0) + 1 FROM experiments;",
                               -1, &stmt, 0) != SQLITE_OK) {
            throw std::runtime_error(std::string("Failed to prepare experiment id query: ")
                                     + sqlite3_errmsg(db_));
        }
        int next_id = 1;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            next_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "exp_%06d", next_id);
        experiment_code = buffer;
    }

    sqlite3_stmt* insert_stmt = 0;
    const char* insert_sql =
        "INSERT INTO experiments("
        "experiment_code, title, artifact_dir, mode, status, created_at, random_seed, options_json"
        ") VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8);";
    if (sqlite3_prepare_v2(db_, insert_sql, -1, &insert_stmt, 0) != SQLITE_OK) {
        throw std::runtime_error(std::string("Failed to prepare experiment insert: ")
                                 + sqlite3_errmsg(db_));
    }

    BindText(insert_stmt, 1, experiment_code);
    BindText(insert_stmt, 2, options.experiment_title);
    BindText(insert_stmt, 3, artifact_dir);
    BindText(insert_stmt, 4, RunModeToString(options.mode));
    BindText(insert_stmt, 5, "running");
    BindText(insert_stmt, 6, created_at);
    if (sqlite3_bind_int64(insert_stmt, 7, options.random_seed) != SQLITE_OK) {
        sqlite3_finalize(insert_stmt);
        throw std::runtime_error("Failed to bind random_seed");
    }
    const std::string options_json = options.ToJson().dump();
    BindText(insert_stmt, 8, options_json);

    if (sqlite3_step(insert_stmt) != SQLITE_DONE) {
        const std::string err = sqlite3_errmsg(db_);
        sqlite3_finalize(insert_stmt);
        throw std::runtime_error("Failed to insert experiment row: " + err);
    }

    sqlite3_finalize(insert_stmt);
    return experiment_code;
}

void MetadataRepository::update_experiment_status(const std::string& experiment_code,
                                                  const std::string& status,
                                                  const std::string& completed_at) const {
    if (!db_) {
        throw std::runtime_error("Metadata repository is not open");
    }

    sqlite3_stmt* stmt = 0;
    const char* sql =
        "UPDATE experiments SET status = ?1, completed_at = ?2 WHERE experiment_code = ?3;";
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, 0) != SQLITE_OK) {
        throw std::runtime_error(std::string("Failed to prepare experiment update: ")
                                 + sqlite3_errmsg(db_));
    }

    BindText(stmt, 1, status);
    BindText(stmt, 2, completed_at);
    BindText(stmt, 3, experiment_code);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        const std::string err = sqlite3_errmsg(db_);
        sqlite3_finalize(stmt);
        throw std::runtime_error("Failed to update experiment row: " + err);
    }
    sqlite3_finalize(stmt);
}

void MetadataRepository::close() {
    if (db_) {
        sqlite3_close(db_);
        db_ = 0;
    }
}

} // namespace teknegram
