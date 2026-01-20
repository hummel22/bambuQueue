#include "app/DatabaseManager.h"

#include <sqlite3.h>
#include <wx/filename.h>
#include <wx/log.h>

namespace {
constexpr int kSchemaVersion = 1;
}  // namespace

DatabaseManager::DatabaseManager() : db_(nullptr) {}

DatabaseManager::~DatabaseManager() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool DatabaseManager::Initialize(const wxString &data_dir, wxString *error_message) {
    db_path_ = wxFileName(data_dir, "bambu_queue.db").GetFullPath();

    if (sqlite3_open(db_path_.utf8_str(), &db_) != SQLITE_OK) {
        if (error_message) {
            *error_message = wxString::Format("Unable to open database at %s", db_path_);
        }
        wxLogError("DatabaseManager: failed to open sqlite database at %s", db_path_);
        return false;
    }

    if (!RunMigrations(error_message)) {
        wxLogError("DatabaseManager: failed to run schema migrations.");
        return false;
    }

    return true;
}

bool DatabaseManager::RunMigrations(wxString *error_message) {
    if (!ExecuteStatement("BEGIN TRANSACTION;", error_message)) {
        return false;
    }

    const wxString create_jobs =
        "CREATE TABLE IF NOT EXISTS jobs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT,"
        "status TEXT,"
        "printer_id INTEGER,"
        "file_path TEXT,"
        "created_at TEXT,"
        "updated_at TEXT"
        ");";
    const wxString create_printers =
        "CREATE TABLE IF NOT EXISTS printers ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL,"
        "host TEXT NOT NULL,"
        "created_at TEXT"
        ");";
    const wxString create_settings =
        "CREATE TABLE IF NOT EXISTS settings ("
        "key TEXT PRIMARY KEY,"
        "value TEXT"
        ");";
    const wxString create_schema_version =
        "CREATE TABLE IF NOT EXISTS schema_version ("
        "version INTEGER NOT NULL"
        ");";

    if (!ExecuteStatement(create_jobs, error_message) ||
        !ExecuteStatement(create_printers, error_message) ||
        !ExecuteStatement(create_settings, error_message) ||
        !ExecuteStatement(create_schema_version, error_message)) {
        ExecuteStatement("ROLLBACK;", nullptr);
        return false;
    }

    if (!EnsureSchemaVersion(error_message)) {
        ExecuteStatement("ROLLBACK;", nullptr);
        return false;
    }

    if (!ExecuteStatement("COMMIT;", error_message)) {
        ExecuteStatement("ROLLBACK;", nullptr);
        return false;
    }

    return true;
}

bool DatabaseManager::ExecuteStatement(const wxString &statement, wxString *error_message) {
    char *err_msg = nullptr;
    if (sqlite3_exec(db_, statement.utf8_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
        if (error_message) {
            *error_message = wxString::Format("Database error: %s", err_msg ? err_msg : "unknown");
        }
        wxLogError("DatabaseManager: SQL error: %s", err_msg ? err_msg : "unknown");
        if (err_msg) {
            sqlite3_free(err_msg);
        }
        return false;
    }
    return true;
}

bool DatabaseManager::EnsureSchemaVersion(wxString *error_message) {
    const char *query = "SELECT version FROM schema_version ORDER BY version DESC LIMIT 1;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        if (error_message) {
            *error_message = "Database error: unable to read schema version.";
        }
        wxLogError("DatabaseManager: unable to prepare schema version statement.");
        if (stmt) {
            sqlite3_finalize(stmt);
        }
        return false;
    }

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const int version = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        if (version != kSchemaVersion) {
            if (error_message) {
                *error_message = "Database schema version mismatch.";
            }
            wxLogError("DatabaseManager: schema version mismatch (expected %d).",
                       kSchemaVersion);
            return false;
        }
        return true;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        if (error_message) {
            *error_message = "Database error: unable to read schema version.";
        }
        wxLogError("DatabaseManager: schema version query failed.");
        return false;
    }

    const wxString insert_version =
        wxString::Format("INSERT INTO schema_version (version) VALUES (%d);", kSchemaVersion);
    return ExecuteStatement(insert_version, error_message);
}
