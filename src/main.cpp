#include <wx/wx.h>

class BambuQueueApp final : public wxApp {
public:
    bool OnInit() override;
};

class BambuQueueFrame final : public wxFrame {
public:
    BambuQueueFrame();
};

bool BambuQueueApp::OnInit() {
    if (!wxApp::OnInit()) {
        return false;
    }

    auto *frame = new BambuQueueFrame();
    frame->Show(true);
    return true;
}

BambuQueueFrame::BambuQueueFrame()
    : wxFrame(nullptr, wxID_ANY, "Bambu Queue", wxDefaultPosition, wxSize(800, 600)) {}

wxIMPLEMENT_APP(BambuQueueApp);
