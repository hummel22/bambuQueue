#include "app/DatabaseManager.h"

#include <sqlite3.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/log.h>

namespace {
constexpr int kSchemaVersion = 2;

constexpr const char kCompletedStatusName[] = "completed";
constexpr const char kRunningStatusName[] = "running";
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

    if (!ExecuteStatement("PRAGMA foreign_keys = ON;", error_message)) {
        wxLogError("DatabaseManager: failed to enable foreign keys.");
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

    const wxString create_statuses =
        "CREATE TABLE IF NOT EXISTS statuses ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT NOT NULL UNIQUE,"
        "is_completed INTEGER NOT NULL DEFAULT 0,"
        "is_terminal INTEGER NOT NULL DEFAULT 0,"
        "created_at TEXT"
        ");";
    const wxString create_jobs =
        "CREATE TABLE IF NOT EXISTS jobs ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "name TEXT,"
        "status_id INTEGER,"
        "status TEXT,"
        "printer_id INTEGER,"
        "file_path TEXT,"
        "thumbnail_path TEXT,"
        "metadata TEXT,"
        "created_at TEXT,"
        "updated_at TEXT,"
        "started_at TEXT,"
        "completed_at TEXT,"
        "FOREIGN KEY(status_id) REFERENCES statuses(id),"
        "FOREIGN KEY(printer_id) REFERENCES printers(id)"
        ");";
    const wxString create_plates =
        "CREATE TABLE IF NOT EXISTS plates ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "job_id INTEGER NOT NULL,"
        "plate_index INTEGER NOT NULL,"
        "name TEXT,"
        "status_id INTEGER,"
        "FOREIGN KEY(job_id) REFERENCES jobs(id) ON DELETE CASCADE,"
        "FOREIGN KEY(status_id) REFERENCES statuses(id),"
        "UNIQUE(job_id, plate_index)"
        ");";
    const wxString create_filaments =
        "CREATE TABLE IF NOT EXISTS filaments ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "job_id INTEGER NOT NULL,"
        "plate_id INTEGER,"
        "slot INTEGER,"
        "material TEXT,"
        "color_hex TEXT,"
        "brand TEXT,"
        "metadata TEXT,"
        "FOREIGN KEY(job_id) REFERENCES jobs(id) ON DELETE CASCADE,"
        "FOREIGN KEY(plate_id) REFERENCES plates(id) ON DELETE SET NULL"
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

    if (!ExecuteStatement(create_statuses, error_message) ||
        !ExecuteStatement(create_printers, error_message) ||
        !ExecuteStatement(create_jobs, error_message) ||
        !ExecuteStatement(create_plates, error_message) ||
        !ExecuteStatement(create_filaments, error_message) ||
        !ExecuteStatement(create_settings, error_message) ||
        !ExecuteStatement(create_schema_version, error_message)) {
        ExecuteStatement("ROLLBACK;", nullptr);
        return false;
    }

    const wxString seed_statuses =
        "INSERT OR IGNORE INTO statuses (name, is_completed, is_terminal, created_at) VALUES "
        "('queued', 0, 0, datetime('now')),"
        "('running', 0, 0, datetime('now')),"
        "('completed', 1, 1, datetime('now')),"
        "('failed', 0, 1, datetime('now')),"
        "('cancelled', 0, 1, datetime('now'));";
    if (!ExecuteStatement(seed_statuses, error_message)) {
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

bool DatabaseManager::ExecuteStatementAllowDuplicateColumn(const wxString &statement,
                                                           wxString *error_message) {
    char *err_msg = nullptr;
    if (sqlite3_exec(db_, statement.utf8_str(), nullptr, nullptr, &err_msg) != SQLITE_OK) {
        const wxString err_text = err_msg ? wxString::FromUTF8(err_msg) : "unknown";
        if (err_text.Contains("duplicate column name")) {
            if (err_msg) {
                sqlite3_free(err_msg);
            }
            return true;
        }
        if (error_message) {
            *error_message = wxString::Format("Database error: %s", err_text);
        }
        wxLogError("DatabaseManager: SQL error: %s", err_text);
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
        if (version < kSchemaVersion) {
            if (version == 1) {
                if (!ExecuteStatement("CREATE TABLE IF NOT EXISTS statuses ("
                                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                      "name TEXT NOT NULL UNIQUE,"
                                      "is_completed INTEGER NOT NULL DEFAULT 0,"
                                      "is_terminal INTEGER NOT NULL DEFAULT 0,"
                                      "created_at TEXT"
                                      ");",
                                      error_message) ||
                    !ExecuteStatement("CREATE TABLE IF NOT EXISTS plates ("
                                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                      "job_id INTEGER NOT NULL,"
                                      "plate_index INTEGER NOT NULL,"
                                      "name TEXT,"
                                      "status_id INTEGER,"
                                      "FOREIGN KEY(job_id) REFERENCES jobs(id) "
                                      "ON DELETE CASCADE,"
                                      "FOREIGN KEY(status_id) REFERENCES statuses(id),"
                                      "UNIQUE(job_id, plate_index)"
                                      ");",
                                      error_message) ||
                    !ExecuteStatement("CREATE TABLE IF NOT EXISTS filaments ("
                                      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                      "job_id INTEGER NOT NULL,"
                                      "plate_id INTEGER,"
                                      "slot INTEGER,"
                                      "material TEXT,"
                                      "color_hex TEXT,"
                                      "brand TEXT,"
                                      "metadata TEXT,"
                                      "FOREIGN KEY(job_id) REFERENCES jobs(id) "
                                      "ON DELETE CASCADE,"
                                      "FOREIGN KEY(plate_id) REFERENCES plates(id) "
                                      "ON DELETE SET NULL"
                                      ");",
                                      error_message)) {
                    return false;
                }

                if (!ExecuteStatementAllowDuplicateColumn(
                        "ALTER TABLE jobs ADD COLUMN status_id INTEGER;", error_message) ||
                    !ExecuteStatementAllowDuplicateColumn(
                        "ALTER TABLE jobs ADD COLUMN thumbnail_path TEXT;", error_message) ||
                    !ExecuteStatementAllowDuplicateColumn(
                        "ALTER TABLE jobs ADD COLUMN metadata TEXT;", error_message) ||
                    !ExecuteStatementAllowDuplicateColumn(
                        "ALTER TABLE jobs ADD COLUMN started_at TEXT;", error_message) ||
                    !ExecuteStatementAllowDuplicateColumn(
                        "ALTER TABLE jobs ADD COLUMN completed_at TEXT;", error_message)) {
                    return false;
                }

                const wxString seed_statuses =
                    "INSERT OR IGNORE INTO statuses "
                    "(name, is_completed, is_terminal, created_at) VALUES "
                    "('queued', 0, 0, datetime('now')),"
                    "('running', 0, 0, datetime('now')),"
                    "('completed', 1, 1, datetime('now')),"
                    "('failed', 0, 1, datetime('now')),"
                    "('cancelled', 0, 1, datetime('now'));";
                if (!ExecuteStatement(seed_statuses, error_message)) {
                    return false;
                }

                if (!ExecuteStatement(
                        "UPDATE jobs SET status_id = (SELECT id FROM statuses WHERE "
                        "statuses.name = jobs.status) WHERE status_id IS NULL "
                        "AND status IS NOT NULL;",
                        error_message)) {
                    return false;
                }
            } else {
                if (error_message) {
                    *error_message = "Database schema version mismatch.";
                }
                wxLogError("DatabaseManager: schema version mismatch (expected %d).",
                           kSchemaVersion);
                return false;
            }

            const wxString update_version =
                wxString::Format("INSERT INTO schema_version (version) VALUES (%d);",
                                 kSchemaVersion);
            return ExecuteStatement(update_version, error_message);
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

bool DatabaseManager::EnsureStatusExists(const wxString &status_name,
                                         bool is_completed,
                                         bool is_terminal,
                                         int *status_id,
                                         wxString *error_message) {
    StatusRecord status;
    if (LookupStatusByName(status_name, &status, nullptr)) {
        if (status_id) {
            *status_id = status.id;
        }
        return true;
    }

    sqlite3_stmt *stmt = nullptr;
    const char *insert_sql =
        "INSERT INTO statuses (name, is_completed, is_terminal, created_at) "
        "VALUES (?, ?, ?, datetime('now'));";
    int rc = sqlite3_prepare_v2(db_, insert_sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        if (error_message) {
            *error_message = "Database error: unable to create status row.";
        }
        wxLogError("DatabaseManager: unable to prepare status insert.");
        if (stmt) {
            sqlite3_finalize(stmt);
        }
        return false;
    }

    sqlite3_bind_text(stmt, 1, status_name.utf8_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, is_completed ? 1 : 0);
    sqlite3_bind_int(stmt, 3, is_terminal ? 1 : 0);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        if (error_message) {
            *error_message = "Database error: unable to insert status row.";
        }
        wxLogError("DatabaseManager: status insert failed.");
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);

    return LookupStatusByName(status_name, &status, error_message);
}

bool DatabaseManager::LookupStatusByName(const wxString &status_name,
                                         StatusRecord *status,
                                         wxString *error_message) {
    const char *query =
        "SELECT id, name, is_completed, is_terminal, created_at FROM statuses WHERE name = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        if (error_message) {
            *error_message = "Database error: unable to read status.";
        }
        wxLogError("DatabaseManager: unable to prepare status lookup.");
        if (stmt) {
            sqlite3_finalize(stmt);
        }
        return false;
    }

    sqlite3_bind_text(stmt, 1, status_name.utf8_str(), -1, SQLITE_TRANSIENT);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        if (status) {
            status->id = sqlite3_column_int(stmt, 0);
            status->name = wxString::FromUTF8(
                reinterpret_cast<const char *>(sqlite3_column_text(stmt, 1)));
            status->is_completed = sqlite3_column_int(stmt, 2) != 0;
            status->is_terminal = sqlite3_column_int(stmt, 3) != 0;
            status->created_at = wxString::FromUTF8(
                reinterpret_cast<const char *>(sqlite3_column_text(stmt, 4)));
        }
        sqlite3_finalize(stmt);
        return true;
    }

    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        if (error_message) {
            *error_message = "Database error: unable to read status.";
        }
        wxLogError("DatabaseManager: status lookup failed.");
        return false;
    }

    return false;
}

bool DatabaseManager::MoveJobAssetsIfNeeded(const wxString &file_path,
                                            const wxString &thumbnail_path,
                                            const wxString &target_dir,
                                            wxString *updated_file_path,
                                            wxString *updated_thumbnail_path,
                                            wxString *error_message) {
    auto move_asset = [&](const wxString &current_path, wxString *updated_path) -> bool {
        if (current_path.empty()) {
            if (updated_path) {
                updated_path->clear();
            }
            return true;
        }

        const wxFileName current_file(current_path);
        const wxString destination_path =
            wxFileName(target_dir, current_file.GetFullName()).GetFullPath();
        if (updated_path) {
            *updated_path = destination_path;
        }
        if (current_path == destination_path) {
            return true;
        }
        if (!current_file.FileExists()) {
            if (error_message) {
                *error_message = wxString::Format("Missing job asset: %s", current_path);
            }
            wxLogError("DatabaseManager: expected job asset missing at %s", current_path);
            return false;
        }
        if (!wxRenameFile(current_path, destination_path, true)) {
            if (error_message) {
                *error_message =
                    wxString::Format("Failed to move job asset to %s", destination_path);
            }
            wxLogError("DatabaseManager: failed to move job asset from %s to %s",
                       current_path,
                       destination_path);
            return false;
        }
        return true;
    };

    return move_asset(file_path, updated_file_path) &&
           move_asset(thumbnail_path, updated_thumbnail_path);
}

bool DatabaseManager::UpdateJobStatus(int job_id,
                                      const wxString &status_name,
                                      const wxString &jobs_dir,
                                      const wxString &completed_dir,
                                      wxString *error_message) {
    const char *query =
        "SELECT jobs.status_id, jobs.status, jobs.file_path, jobs.thumbnail_path, "
        "COALESCE(statuses.is_completed, 0) "
        "FROM jobs LEFT JOIN statuses ON jobs.status_id = statuses.id WHERE jobs.id = ?;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        if (error_message) {
            *error_message = "Database error: unable to read job.";
        }
        wxLogError("DatabaseManager: unable to prepare job lookup.");
        if (stmt) {
            sqlite3_finalize(stmt);
        }
        return false;
    }

    sqlite3_bind_int(stmt, 1, job_id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        if (error_message) {
            *error_message = "Database error: job not found.";
        }
        wxLogError("DatabaseManager: job %d not found.", job_id);
        return false;
    }

    const int current_status_id = sqlite3_column_int(stmt, 0);
    const wxString current_status_name = sqlite3_column_text(stmt, 1)
                                             ? wxString::FromUTF8(reinterpret_cast<const char *>(
                                                   sqlite3_column_text(stmt, 1)))
                                             : wxEmptyString;
    const wxString current_file_path = sqlite3_column_text(stmt, 2)
                                           ? wxString::FromUTF8(reinterpret_cast<const char *>(
                                                 sqlite3_column_text(stmt, 2)))
                                           : wxEmptyString;
    const wxString current_thumbnail_path =
        sqlite3_column_text(stmt, 3)
            ? wxString::FromUTF8(reinterpret_cast<const char *>(sqlite3_column_text(stmt, 3)))
            : wxEmptyString;
    const bool current_is_completed = sqlite3_column_int(stmt, 4) != 0;
    sqlite3_finalize(stmt);

    const bool new_is_completed =
        status_name.CmpNoCase(kCompletedStatusName) == 0;
    const bool new_is_terminal = new_is_completed ||
                                 status_name.CmpNoCase("failed") == 0 ||
                                 status_name.CmpNoCase("cancelled") == 0;

    int new_status_id = 0;
    if (!EnsureStatusExists(status_name, new_is_completed, new_is_terminal, &new_status_id,
                            error_message)) {
        return false;
    }

    wxString updated_file_path = current_file_path;
    wxString updated_thumbnail_path = current_thumbnail_path;
    if (new_is_completed != current_is_completed) {
        const wxString &target_dir = new_is_completed ? completed_dir : jobs_dir;
        if (!MoveJobAssetsIfNeeded(current_file_path,
                                   current_thumbnail_path,
                                   target_dir,
                                   &updated_file_path,
                                   &updated_thumbnail_path,
                                   error_message)) {
            return false;
        }
    }

    const char *update_sql =
        "UPDATE jobs SET status = ?, status_id = ?, file_path = ?, thumbnail_path = ?, "
        "updated_at = datetime('now'), "
        "started_at = CASE WHEN ? = 1 AND started_at IS NULL THEN datetime('now') "
        "ELSE started_at END, "
        "completed_at = CASE WHEN ? = 1 THEN datetime('now') ELSE NULL END "
        "WHERE id = ?;";
    rc = sqlite3_prepare_v2(db_, update_sql, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        if (error_message) {
            *error_message = "Database error: unable to update job status.";
        }
        wxLogError("DatabaseManager: unable to prepare job update.");
        if (stmt) {
            sqlite3_finalize(stmt);
        }
        return false;
    }

    sqlite3_bind_text(stmt, 1, status_name.utf8_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, new_status_id);
    sqlite3_bind_text(stmt, 3, updated_file_path.utf8_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, updated_thumbnail_path.utf8_str(), -1, SQLITE_TRANSIENT);
    const bool is_running = status_name.CmpNoCase(kRunningStatusName) == 0;
    sqlite3_bind_int(stmt, 5, is_running ? 1 : 0);
    sqlite3_bind_int(stmt, 6, new_is_completed ? 1 : 0);
    sqlite3_bind_int(stmt, 7, job_id);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        if (error_message) {
            *error_message = "Database error: unable to update job status.";
        }
        wxLogError("DatabaseManager: job status update failed.");
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);

    const wxString current_display =
        current_status_name.empty()
            ? wxString::Format("id:%d", current_status_id)
            : current_status_name;
    wxLogMessage("DatabaseManager: job %d status changed from %s to %s (status_id %d -> %d).",
                 job_id,
                 current_display,
                 status_name,
                 current_status_id,
                 new_status_id);
    return true;
}

bool DatabaseManager::GetCompletedJobsOrdered(std::vector<JobRecord> *jobs,
                                              wxString *error_message) {
    if (!jobs) {
        if (error_message) {
            *error_message = "Internal error: jobs storage unavailable.";
        }
        wxLogError("DatabaseManager: jobs storage unavailable.");
        return false;
    }

    jobs->clear();
    const char *query =
        "SELECT jobs.id, jobs.name, jobs.status_id, statuses.name, jobs.printer_id, "
        "jobs.file_path, jobs.thumbnail_path, jobs.metadata, jobs.created_at, "
        "jobs.updated_at, jobs.started_at, jobs.completed_at "
        "FROM jobs "
        "JOIN statuses ON jobs.status_id = statuses.id "
        "WHERE statuses.is_completed = 1 "
        "ORDER BY jobs.started_at ASC, jobs.id ASC;";
    sqlite3_stmt *stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, query, -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        if (error_message) {
            *error_message = "Database error: unable to read completed jobs.";
        }
        wxLogError("DatabaseManager: unable to prepare completed jobs query.");
        if (stmt) {
            sqlite3_finalize(stmt);
        }
        return false;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        auto column_text = [&](int column) -> wxString {
            const unsigned char *text = sqlite3_column_text(stmt, column);
            return text ? wxString::FromUTF8(reinterpret_cast<const char *>(text))
                        : wxEmptyString;
        };
        JobRecord job;
        job.id = sqlite3_column_int(stmt, 0);
        job.name = column_text(1);
        job.status_id = sqlite3_column_int(stmt, 2);
        job.status_name = column_text(3);
        job.printer_id = sqlite3_column_int(stmt, 4);
        job.file_path = column_text(5);
        job.thumbnail_path = column_text(6);
        job.metadata = column_text(7);
        job.created_at = column_text(8);
        job.updated_at = column_text(9);
        job.started_at = column_text(10);
        job.completed_at = column_text(11);
        jobs->push_back(job);
    }

    if (rc != SQLITE_DONE) {
        if (error_message) {
            *error_message = "Database error: unable to read completed jobs.";
        }
        wxLogError("DatabaseManager: completed jobs query failed.");
        sqlite3_finalize(stmt);
        return false;
    }

    sqlite3_finalize(stmt);
    return true;
}
