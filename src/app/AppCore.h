#pragma once

#include <wx/string.h>

#include <vector>

struct PrinterDefinition {
    wxString name;
    wxString host;
};

struct AppConfig {
    wxString data_dir;
    wxString jobs_dir;
    wxString completed_dir;
    wxString import_dir;
    std::vector<PrinterDefinition> printers;
};

class AppCore {
public:
    bool Initialize(wxString *error_message);
    const AppConfig &GetConfig() const;

private:
    bool LoadConfig(wxString *error_message);
    bool SaveConfig(wxString *error_message);
    bool EnsureDirectories(wxString *error_message) const;
    void SetDefaultPaths(const wxString &base_dir);

    wxString config_path_;
    AppConfig config_;
};
