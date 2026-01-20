#pragma once

#include <wx/string.h>

#include <vector>

struct sqlite3;

struct JobRecord {
    int id = 0;
    wxString name;
    int status_id = 0;
    wxString status_name;
    int printer_id = 0;
    wxString file_path;
    wxString thumbnail_path;
    wxString metadata;
    wxString created_at;
    wxString updated_at;
    wxString started_at;
    wxString completed_at;
};

struct PlateRecord {
    int id = 0;
    int job_id = 0;
    int plate_index = 0;
    wxString name;
    int status_id = 0;
};

struct PlateDefinition {
    int plate_index = 0;
    wxString name;
};

struct FilamentRecord {
    int id = 0;
    int job_id = 0;
    int plate_id = 0;
    int slot = 0;
    wxString material;
    wxString color_hex;
    wxString brand;
    wxString metadata;
};

struct PrinterRecord {
    int id = 0;
    wxString name;
    wxString host;
    wxString created_at;
};

struct StatusRecord {
    int id = 0;
    wxString name;
    bool is_completed = false;
    bool is_terminal = false;
    wxString created_at;
};

class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    bool Initialize(const wxString &data_dir, wxString *error_message);
    bool InsertImportedJob(const wxString &name,
                           const wxString &file_path,
                           const wxString &thumbnail_path,
                           const wxString &metadata,
                           const std::vector<PlateDefinition> &plates,
                           int *job_id,
                           wxString *error_message);
    bool UpdateJobStatus(int job_id,
                         const wxString &status_name,
                         const wxString &jobs_dir,
                         const wxString &completed_dir,
                         wxString *error_message);
    bool GetCompletedJobsOrdered(std::vector<JobRecord> *jobs, wxString *error_message);
    bool JobExistsForFile(const wxString &file_path);

private:
    bool RunMigrations(wxString *error_message);
    bool ExecuteStatement(const wxString &statement, wxString *error_message);
    bool ExecuteStatementAllowDuplicateColumn(const wxString &statement, wxString *error_message);
    bool EnsureSchemaVersion(wxString *error_message);
    bool EnsureStatusExists(const wxString &status_name,
                            bool is_completed,
                            bool is_terminal,
                            int *status_id,
                            wxString *error_message);
    bool LookupStatusByName(const wxString &status_name,
                            StatusRecord *status,
                            wxString *error_message);
    bool MoveJobAssetsIfNeeded(const wxString &file_path,
                               const wxString &thumbnail_path,
                               const wxString &target_dir,
                               wxString *updated_file_path,
                               wxString *updated_thumbnail_path,
                               wxString *error_message);

    sqlite3 *db_;
    wxString db_path_;
};
