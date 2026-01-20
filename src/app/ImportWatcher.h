#pragma once

#include "app/AppConfig.h"
#include "app/DatabaseManager.h"
#include "app/ThreeMfImporter.h"

#include <wx/datetime.h>
#include <wx/timer.h>

#include <unordered_map>

struct PendingFileInfo {
    wxLongLong size;
    wxDateTime modified_time;
    int stable_checks = 0;
};

class ImportWatcher : public wxEvtHandler {
public:
    ImportWatcher(const AppConfig &config, DatabaseManager &database);
    ~ImportWatcher();

    bool Start(wxString *error_message);

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
