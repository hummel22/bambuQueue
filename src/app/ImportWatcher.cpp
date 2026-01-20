#include "app/ImportWatcher.h"

#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/log.h>

#include <algorithm>

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

size_t ImportWatcher::GetReadyImportCount() const {
    size_t count = 0;
    for (const auto &entry : pending_files_) {
        if (entry.second.ready) {
            count += 1;
        }
    }
    return count;
}

std::vector<ImportCandidate> ImportWatcher::GetReadyImports() const {
    std::vector<ImportCandidate> candidates;
    candidates.reserve(pending_files_.size());
    for (const auto &entry : pending_files_) {
        if (!entry.second.ready) {
            continue;
        }
        wxFileName file(wxString::FromUTF8(entry.first.c_str()));
        ImportCandidate candidate;
        candidate.path = file.GetFullPath();
        candidate.display_name = file.GetFullName();
        candidates.push_back(candidate);
    }
    std::sort(candidates.begin(),
              candidates.end(),
              [](const ImportCandidate &left, const ImportCandidate &right) {
                  return left.display_name.CmpNoCase(right.display_name) < 0;
              });
    return candidates;
}

bool ImportWatcher::ImportFiles(const std::vector<wxString> &paths, wxString *error_message) {
    wxString last_error;
    bool any_failure = false;
    for (const auto &path : paths) {
        if (path.empty()) {
            continue;
        }
        if (!importer_.ImportFile(path, &last_error)) {
            any_failure = true;
            wxLogWarning("ImportWatcher: failed to import %s (%s)", path, last_error);
            continue;
        }
        pending_files_.erase(path.ToStdString());
    }

    if (any_failure && error_message) {
        *error_message = last_error.empty() ? "Unable to import one or more jobs." : last_error;
    }
    return !any_failure;
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
            pending.ready = true;
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
