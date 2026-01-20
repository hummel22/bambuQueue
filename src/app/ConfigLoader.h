#pragma once

#include "app/AppConfig.h"

#include <wx/string.h>

class ConfigLoader {
public:
    bool LoadOrCreate(const wxString &base_dir, AppConfig *config, wxString *error_message);
    const wxString &GetConfigPath() const;

private:
    void SetDefaultPaths(const wxString &base_dir, AppConfig *config) const;
    bool LoadConfig(AppConfig *config, wxString *error_message);
    bool SaveConfig(const AppConfig &config, wxString *error_message) const;
    bool ValidateConfig(const AppConfig &config, wxString *error_message) const;

    wxString config_path_;
};
