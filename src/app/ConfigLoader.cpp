#include "app/ConfigLoader.h"

#include <wx/fileconf.h>
#include <wx/filename.h>
#include <wx/log.h>

namespace {
constexpr const char kConfigFileName[] = "config.ini";
}  // namespace

bool ConfigLoader::LoadOrCreate(const wxString &base_dir,
                                AppConfig *config,
                                wxString *error_message) {
    if (!config) {
        if (error_message) {
            *error_message = "Internal error: configuration storage unavailable.";
        }
        wxLogError("ConfigLoader: configuration storage unavailable.");
        return false;
    }

    config_path_ = wxFileName(base_dir, kConfigFileName).GetFullPath();
    SetDefaultPaths(base_dir, config);

    const bool has_config = wxFileName::FileExists(config_path_);
    if (has_config) {
        if (!LoadConfig(config, error_message)) {
            wxLogError("ConfigLoader: failed to load config from %s", config_path_);
            return false;
        }
    } else {
        wxLogWarning("ConfigLoader: config file missing, creating defaults at %s",
                     config_path_);
    }

    if (!ValidateConfig(*config, error_message)) {
        wxLogError("ConfigLoader: invalid configuration detected.");
        return false;
    }

    if (!has_config) {
        if (!SaveConfig(*config, error_message)) {
            wxLogError("ConfigLoader: unable to save default config to %s", config_path_);
            return false;
        }
    }

    return true;
}

const wxString &ConfigLoader::GetConfigPath() const {
    return config_path_;
}

void ConfigLoader::SetDefaultPaths(const wxString &base_dir, AppConfig *config) const {
    config->data_dir = base_dir;
    config->jobs_dir = wxFileName(base_dir, "jobs").GetFullPath();
    config->completed_dir = wxFileName(base_dir, "completed").GetFullPath();
    config->import_dir = wxFileName(base_dir, "import").GetFullPath();
}

bool ConfigLoader::LoadConfig(AppConfig *config, wxString *error_message) {
    wxFileConfig file_config(
        wxEmptyString, wxEmptyString, config_path_, wxEmptyString, wxCONFIG_USE_LOCAL_FILE);

    file_config.Read("paths/data_dir", &config->data_dir, config->data_dir);
    file_config.Read("paths/jobs_dir", &config->jobs_dir, config->jobs_dir);
    file_config.Read("paths/completed_dir", &config->completed_dir, config->completed_dir);
    file_config.Read("paths/import_dir", &config->import_dir, config->import_dir);

    config->printers.clear();
    file_config.SetPath("/printers");
    long count = 0;
    file_config.Read("count", &count, 0L);
    for (long index = 0; index < count; ++index) {
        const wxString path = wxString::Format("/printers/%ld", index);
        file_config.SetPath(path);

        PrinterDefinition printer;
        file_config.Read("name", &printer.name, wxEmptyString);
        file_config.Read("host", &printer.host, wxEmptyString);
        file_config.Read("access_code", &printer.access_code, wxEmptyString);
        file_config.Read("serial", &printer.serial, wxEmptyString);
        if (!printer.name.empty() || !printer.host.empty()) {
            config->printers.push_back(printer);
        }
    }

    return true;
}

bool ConfigLoader::SaveConfig(const AppConfig &config, wxString *error_message) const {
    wxFileConfig file_config(
        wxEmptyString, wxEmptyString, config_path_, wxEmptyString, wxCONFIG_USE_LOCAL_FILE);

    file_config.DeleteAll();
    file_config.Write("paths/data_dir", config.data_dir);
    file_config.Write("paths/jobs_dir", config.jobs_dir);
    file_config.Write("paths/completed_dir", config.completed_dir);
    file_config.Write("paths/import_dir", config.import_dir);

    file_config.SetPath("/printers");
    file_config.Write("count", static_cast<long>(config.printers.size()));
    for (size_t index = 0; index < config.printers.size(); ++index) {
        const wxString path = wxString::Format("/printers/%zu", index);
        file_config.SetPath(path);
        file_config.Write("name", config.printers[index].name);
        file_config.Write("host", config.printers[index].host);
        file_config.Write("access_code", config.printers[index].access_code);
        file_config.Write("serial", config.printers[index].serial);
    }

    if (!file_config.Flush()) {
        if (error_message) {
            *error_message = wxString::Format("Failed to write config file: %s", config_path_);
        }
        wxLogError("ConfigLoader: failed to write config file at %s", config_path_);
        return false;
    }

    return true;
}

bool ConfigLoader::ValidateConfig(const AppConfig &config, wxString *error_message) const {
    if (config.data_dir.empty()) {
        if (error_message) {
            *error_message = "Configuration error: data directory is missing.";
        }
        wxLogError("ConfigLoader: data_dir missing in configuration.");
        return false;
    }
    if (config.jobs_dir.empty()) {
        if (error_message) {
            *error_message = "Configuration error: jobs directory is missing.";
        }
        wxLogError("ConfigLoader: jobs_dir missing in configuration.");
        return false;
    }
    if (config.completed_dir.empty()) {
        if (error_message) {
            *error_message = "Configuration error: completed directory is missing.";
        }
        wxLogError("ConfigLoader: completed_dir missing in configuration.");
        return false;
    }
    if (config.import_dir.empty()) {
        if (error_message) {
            *error_message = "Configuration error: import directory is missing.";
        }
        wxLogError("ConfigLoader: import_dir missing in configuration.");
        return false;
    }
    return true;
}
