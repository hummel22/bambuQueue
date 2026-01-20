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
