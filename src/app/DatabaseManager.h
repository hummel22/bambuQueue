#pragma once

#include <wx/string.h>

struct sqlite3;

class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();

    bool Initialize(const wxString &data_dir, wxString *error_message);

private:
    bool RunMigrations(wxString *error_message);
    bool ExecuteStatement(const wxString &statement, wxString *error_message);
    bool EnsureSchemaVersion(wxString *error_message);

    sqlite3 *db_;
    wxString db_path_;
};
