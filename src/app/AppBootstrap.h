#pragma once

#include "app/AppConfig.h"
#include "app/DatabaseManager.h"

#include <wx/string.h>

class AppBootstrap {
public:
    bool Initialize(wxString *error_message);
    const AppConfig &GetConfig() const;
    DatabaseManager &GetDatabase();

private:
    bool EnsureDirectories(wxString *error_message) const;

    AppConfig config_;
    DatabaseManager database_;
};
