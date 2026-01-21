#pragma once

#include <wx/string.h>

class FtpsClient {
public:
    bool UploadFile(const wxString &host,
                    const wxString &access_code,
                    const wxString &local_path,
                    const wxString &remote_name,
                    wxString *error_message);
};
