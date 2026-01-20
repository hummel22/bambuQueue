#include "app/ImportWatcher.h"

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/log.h>

namespace {
constexpr int kScanIntervalMs = 2000;

bool IsGcode3mfFile(const wxFileName &file) {
    const wxString lower = file.GetFullName().Lower();
    return lower.EndsWith(".gcode.3mf");
}
}  // namespace

ImportWatcher::ImportWatcher(const AppConfig &config, DatabaseManager &database)
    : config_(config),
      database_(database),
      importer_(config, database),
      timer_(this) {}

ImportWatcher::~ImportWatcher() {
    if (timer_.IsRunning()) {
        timer_.Stop();
    }
}

bool ImportWatcher::Start(wxString *error_message) {
    if (config_.import_dir.empty()) {
        if (error_message) {
            *error_message = "Import directory is not configured.";
        }
        wxLogError("ImportWatcher: import directory missing.");
        return false;
    }

    Bind(wxEVT_TIMER, &ImportWatcher::OnTimer, this);
    if (!timer_.Start(kScanIntervalMs)) {
        if (error_message) {
            *error_message = "Unable to start import directory watcher.";
        }
        wxLogError("ImportWatcher: failed to start scan timer.");
        return false;
    }

    ScanImportDirectory();
    return true;
}

void ImportWatcher::OnTimer(wxTimerEvent &event) {
    wxUnusedVar(event);
    ScanImportDirectory();
}

void ImportWatcher::ScanImportDirectory() {
    wxDir dir(config_.import_dir);
    if (!dir.IsOpened()) {
        wxLogWarning("ImportWatcher: unable to open import directory %s",
                     config_.import_dir);
        return;
    }

    std::unordered_map<std::string, bool> seen_files;
    wxString filename;
    bool found = dir.GetFirst(&filename, "*.*", wxDIR_FILES);
    while (found) {
        wxFileName file(config_.import_dir, filename);
        if (!IsGcode3mfFile(file)) {
            found = dir.GetNext(&filename);
            continue;
        }

        const wxString full_path = file.GetFullPath();
        seen_files[full_path.ToStdString()] = true;
        const wxLongLong size = file.GetSize();
        const wxDateTime modified = file.GetModificationTime();

        auto &pending = pending_files_[full_path.ToStdString()];
        if (pending.stable_checks == 0) {
            pending.size = size;
            pending.modified_time = modified;
            pending.stable_checks = 1;
            found = dir.GetNext(&filename);
            continue;
        }

        if (pending.size == size && pending.modified_time == modified) {
            pending.stable_checks += 1;
        } else {
            pending.size = size;
            pending.modified_time = modified;
            pending.stable_checks = 1;
        }

        if (pending.stable_checks >= 2) {
            wxString error_message;
            if (!importer_.ImportFile(full_path, &error_message)) {
                wxLogWarning("ImportWatcher: failed to import %s (%s)",
                             full_path,
                             error_message);
            }
            pending_files_.erase(full_path.ToStdString());
        }

        found = dir.GetNext(&filename);
    }

    CleanupPendingFiles(seen_files);
}

void ImportWatcher::CleanupPendingFiles(
    const std::unordered_map<std::string, bool> &seen_files) {
    for (auto it = pending_files_.begin(); it != pending_files_.end();) {
        if (seen_files.find(it->first) == seen_files.end()) {
            it = pending_files_.erase(it);
        } else {
            ++it;
        }
    }
}
