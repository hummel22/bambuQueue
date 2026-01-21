#include "app/FtpsClient.h"

#include <curl/curl.h>
#include <wx/log.h>
#include <wx/wfstream.h>

namespace {
class CurlGlobal {
public:
    CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobal() { curl_global_cleanup(); }
};

CurlGlobal &EnsureCurlGlobal() {
    static CurlGlobal instance;
    return instance;
}
}  // namespace

bool FtpsClient::UploadFile(const wxString &host,
                            const wxString &access_code,
                            const wxString &local_path,
                            const wxString &remote_name,
                            wxString *error_message) {
    EnsureCurlGlobal();

    if (host.empty() || access_code.empty()) {
        if (error_message) {
            *error_message = "FTPS upload failed: missing host or access code.";
        }
        wxLogError("FtpsClient: missing host or access code.");
        return false;
    }

    wxFileInputStream input(local_path);
    if (!input.IsOk()) {
        if (error_message) {
            *error_message = wxString::Format("FTPS upload failed: unable to open %s", local_path);
        }
        wxLogError("FtpsClient: unable to open %s", local_path);
        return false;
    }

    CURL *curl = curl_easy_init();
    if (!curl) {
        if (error_message) {
            *error_message = "FTPS upload failed: unable to initialize curl.";
        }
        wxLogError("FtpsClient: curl initialization failed.");
        return false;
    }

    wxString url = wxString::Format("ftps://%s:990/%s", host, remote_name);
    wxString userpwd = wxString::Format("bblp:%s", access_code);

    curl_easy_setopt(curl, CURLOPT_URL, url.utf8_str());
    curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd.utf8_str());
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

    const auto size = input.GetLength();
    if (size != wxInvalidOffset) {
        curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(size));
    }

    curl_easy_setopt(curl, CURLOPT_READFUNCTION,
                     +[](char *buffer, size_t size, size_t nitems, void *userdata) -> size_t {
                         auto *stream = static_cast<wxInputStream *>(userdata);
                         const size_t bytes = size * nitems;
                         if (!stream || !stream->CanRead()) {
                             return 0;
                         }
                         return stream->Read(buffer, bytes).LastRead();
                     });
    curl_easy_setopt(curl, CURLOPT_READDATA, &input);

    CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        if (error_message) {
            *error_message = wxString::Format("FTPS upload failed: %s",
                                              curl_easy_strerror(result));
        }
        wxLogError("FtpsClient: upload failed: %s", curl_easy_strerror(result));
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_cleanup(curl);
    wxLogMessage("FtpsClient: uploaded %s to %s", local_path, url);
    return true;
}
