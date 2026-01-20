#include <wx/wx.h>

#include "app/AppCore.h"

#include <memory>

class BambuQueueApp final : public wxApp {
public:
    bool OnInit() override;

private:
    std::unique_ptr<AppCore> app_core_;
};

class BambuQueueFrame final : public wxFrame {
public:
    BambuQueueFrame();
};

bool BambuQueueApp::OnInit() {
    if (!wxApp::OnInit()) {
        return false;
    }

    app_core_ = std::make_unique<AppCore>();
    wxString error_message;
    if (!app_core_->Initialize(&error_message)) {
        wxMessageBox(
            error_message.empty() ? "Unable to initialize application configuration."
                                  : error_message,
            "Configuration Error",
            wxOK | wxICON_ERROR);
        return false;
    }

    auto *frame = new BambuQueueFrame();
    frame->Show(true);
    return true;
}

BambuQueueFrame::BambuQueueFrame()
    : wxFrame(nullptr, wxID_ANY, "Bambu Queue", wxDefaultPosition, wxSize(800, 600)) {}

wxIMPLEMENT_APP(BambuQueueApp);
