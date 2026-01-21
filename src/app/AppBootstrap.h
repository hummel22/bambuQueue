#pragma once

#include "app/AppConfig.h"
#include "app/DatabaseManager.h"
#include "app/ImportWatcher.h"
#include "app/PrinterCoordinator.h"

#include <wx/string.h>

#include <memory>

class AppBootstrap {
public:
    bool Initialize(wxString *error_message);
    const AppConfig &GetConfig() const;
    DatabaseManager &GetDatabase();
    ImportWatcher *GetImportWatcher();

private:
    bool EnsureDirectories(wxString *error_message) const;

    AppConfig config_;
    DatabaseManager database_;
    std::unique_ptr<ImportWatcher> import_watcher_;
    std::unique_ptr<PrinterCoordinator> printer_coordinator_;
};
