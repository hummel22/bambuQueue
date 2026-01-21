#include "app/AppBootstrap.h"

#include "app/ConfigLoader.h"

#include <wx/filename.h>
#include <wx/log.h>
#include <wx/stdpaths.h>

namespace {
bool EnsureDirectory(const wxString &path, wxString *error_message) {
    if (path.empty()) {
        if (error_message) {
            *error_message = "Missing directory path for application data.";
        }
        wxLogError("AppBootstrap: missing directory path while initializing.");
        return false;
    }

    if (wxFileName::DirExists(path)) {
        return true;
    }

    if (wxFileName::Mkdir(path, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
        return true;
    }

    if (error_message) {
        *error_message = wxString::Format("Failed to create required directory: %s", path);
    }
    wxLogError("AppBootstrap: failed to create directory %s", path);
    return false;
}
}  // namespace

bool AppBootstrap::Initialize(wxString *error_message) {
    const wxString base_dir = wxStandardPaths::Get().GetUserDataDir();
    ConfigLoader loader;
    if (!loader.LoadOrCreate(base_dir, &config_, error_message)) {
        wxLogError("AppBootstrap: configuration load failed.");
        return false;
    }

    if (!EnsureDirectories(error_message)) {
        wxLogError("AppBootstrap: failed to initialize data directories.");
        return false;
    }

    if (!database_.Initialize(config_.data_dir, error_message)) {
        wxLogError("AppBootstrap: database initialization failed.");
        return false;
    }

    import_watcher_ = std::make_unique<ImportWatcher>(config_, database_);
    if (!import_watcher_->Start(error_message)) {
        wxLogWarning("AppBootstrap: import watcher failed to start.");
    }

    printer_coordinator_ = std::make_unique<PrinterCoordinator>(config_, database_);
    if (!printer_coordinator_->Start(error_message)) {
        wxLogWarning("AppBootstrap: printer coordinator failed to start.");
    }

    return true;
}

const AppConfig &AppBootstrap::GetConfig() const {
    return config_;
}

DatabaseManager &AppBootstrap::GetDatabase() {
    return database_;
}

ImportWatcher *AppBootstrap::GetImportWatcher() {
    return import_watcher_.get();
}

bool AppBootstrap::EnsureDirectories(wxString *error_message) const {
    return EnsureDirectory(config_.data_dir, error_message) &&
           EnsureDirectory(config_.jobs_dir, error_message) &&
           EnsureDirectory(config_.completed_dir, error_message) &&
           EnsureDirectory(config_.import_dir, error_message);
}
