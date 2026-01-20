#pragma once

#include "app/AppConfig.h"
#include "app/DatabaseManager.h"
#include "app/ThreeMfImporter.h"

#include <wx/datetime.h>
#include <wx/timer.h>

#include <unordered_map>
#include <vector>

struct PendingFileInfo {
    wxLongLong size;
    wxDateTime modified_time;
    int stable_checks = 0;
    bool ready = false;
};

struct ImportCandidate {
    wxString path;
    wxString display_name;
};

class ImportWatcher : public wxEvtHandler {
public:
    ImportWatcher(const AppConfig &config, DatabaseManager &database);
    ~ImportWatcher();

    bool Start(wxString *error_message);
    size_t GetReadyImportCount() const;
    std::vector<ImportCandidate> GetReadyImports() const;
    bool ImportFiles(const std::vector<wxString> &paths, wxString *error_message);

private:
    void OnTimer(wxTimerEvent &event);
    void ScanImportDirectory();
    void CleanupPendingFiles(const std::unordered_map<std::string, bool> &seen_files);

    const AppConfig &config_;
    DatabaseManager &database_;
    ThreeMfImporter importer_;
    wxTimer timer_;
    std::unordered_map<std::string, PendingFileInfo> pending_files_;
};
