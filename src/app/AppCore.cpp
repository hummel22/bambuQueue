#include "AppCore.h"

#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

namespace {
constexpr const char kConfigFileName[] = "config.ini";

bool EnsureDirectory(const wxString &path, wxString *error_message) {
    if (path.empty()) {
        if (error_message) {
            *error_message = "Missing directory path for application data.";
        }
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
    return false;
}
}  // namespace

bool AppCore::Initialize(wxString *error_message) {
    const wxString base_dir = wxStandardPaths::Get().GetUserDataDir();
    config_path_ = wxFileName(base_dir, kConfigFileName).GetFullPath();
    SetDefaultPaths(base_dir);

    const bool has_config = wxFileName::FileExists(config_path_);
    if (has_config) {
        if (!LoadConfig(error_message)) {
            return false;
        }
    }

    if (!EnsureDirectories(error_message)) {
        return false;
    }

    if (!has_config) {
        if (!SaveConfig(error_message)) {
            return false;
        }
    }

    return true;
}

const AppConfig &AppCore::GetConfig() const {
    return config_;
}

bool AppCore::LoadConfig(wxString *error_message) {
    wxFileConfig config(
        wxEmptyString, wxEmptyString, config_path_, wxEmptyString, wxCONFIG_USE_LOCAL_FILE);

    config.Read("paths/data_dir", &config_.data_dir, config_.data_dir);
    config.Read("paths/jobs_dir", &config_.jobs_dir, config_.jobs_dir);
    config.Read("paths/completed_dir", &config_.completed_dir, config_.completed_dir);
    config.Read("paths/import_dir", &config_.import_dir, config_.import_dir);

    config_.printers.clear();
    config.SetPath("/printers");
    long count = 0;
    config.Read("count", &count, 0L);
    for (long index = 0; index < count; ++index) {
        const wxString path = wxString::Format("/printers/%ld", index);
        config.SetPath(path);

        PrinterDefinition printer;
        config.Read("name", &printer.name, wxEmptyString);
        config.Read("host", &printer.host, wxEmptyString);
        if (!printer.name.empty() || !printer.host.empty()) {
            config_.printers.push_back(printer);
        }
    }

    return true;
}

bool AppCore::SaveConfig(wxString *error_message) {
    wxFileConfig config(
        wxEmptyString, wxEmptyString, config_path_, wxEmptyString, wxCONFIG_USE_LOCAL_FILE);

    config.DeleteAll();
    config.Write("paths/data_dir", config_.data_dir);
    config.Write("paths/jobs_dir", config_.jobs_dir);
    config.Write("paths/completed_dir", config_.completed_dir);
    config.Write("paths/import_dir", config_.import_dir);

    config.SetPath("/printers");
    config.Write("count", static_cast<long>(config_.printers.size()));
    for (size_t index = 0; index < config_.printers.size(); ++index) {
        const wxString path = wxString::Format("/printers/%zu", index);
        config.SetPath(path);
        config.Write("name", config_.printers[index].name);
        config.Write("host", config_.printers[index].host);
    }

    if (!config.Flush()) {
        if (error_message) {
            *error_message = wxString::Format("Failed to write config file: %s", config_path_);
        }
        return false;
    }

    return true;
}

bool AppCore::EnsureDirectories(wxString *error_message) const {
    return EnsureDirectory(config_.data_dir, error_message) &&
           EnsureDirectory(config_.jobs_dir, error_message) &&
           EnsureDirectory(config_.completed_dir, error_message) &&
           EnsureDirectory(config_.import_dir, error_message);
}

void AppCore::SetDefaultPaths(const wxString &base_dir) {
    config_.data_dir = base_dir;
    config_.jobs_dir = wxFileName(base_dir, "jobs").GetFullPath();
    config_.completed_dir = wxFileName(base_dir, "completed").GetFullPath();
    config_.import_dir = wxFileName(base_dir, "import").GetFullPath();
}
