#include <wx/checklst.h>
#include <wx/listctrl.h>
#include <wx/statline.h>
#include <wx/wx.h>

#include "app/AppBootstrap.h"

#include <memory>
#include <vector>

class BambuQueueApp final : public wxApp {
public:
    bool OnInit() override;

private:
    std::unique_ptr<AppBootstrap> app_core_;
};

class ImportDialog final : public wxDialog {
public:
    ImportDialog(wxWindow *parent, ImportWatcher &import_watcher);

private:
    void PopulateCandidates();
    void OnImport(wxCommandEvent &event);

    ImportWatcher &import_watcher_;
    wxCheckListBox *job_list_ = nullptr;
    wxRadioButton *back_of_queue_ = nullptr;
    std::vector<ImportCandidate> candidates_;
};

class BambuQueueFrame final : public wxFrame {
public:
    explicit BambuQueueFrame(AppBootstrap &app_core);

private:
    void OnImportClicked(wxCommandEvent &event);
    void OnImportTimer(wxTimerEvent &event);
    void UpdateImportBadge();

    AppBootstrap &app_core_;
    wxButton *import_button_ = nullptr;
    wxStaticText *import_badge_ = nullptr;
    wxTimer import_timer_;
};

bool BambuQueueApp::OnInit() {
    if (!wxApp::OnInit()) {
        return false;
    }

    app_core_ = std::make_unique<AppBootstrap>();
    wxString error_message;
    if (!app_core_->Initialize(&error_message)) {
        wxMessageBox(
            error_message.empty() ? "Unable to initialize application configuration."
                                  : error_message,
            "Configuration Error",
            wxOK | wxICON_ERROR);
        return false;
    }

    auto *frame = new BambuQueueFrame(*app_core_);
    frame->Show(true);
    return true;
}

ImportDialog::ImportDialog(wxWindow *parent, ImportWatcher &import_watcher)
    : wxDialog(parent,
               wxID_ANY,
               "Import jobs",
               wxDefaultPosition,
               wxSize(520, 400),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      import_watcher_(import_watcher) {
    wxPanel *panel = new wxPanel(this);
    panel->SetBackgroundColour(wxColour("#F5F6F7"));

    wxBoxSizer *root = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *content = new wxBoxSizer(wxVERTICAL);

    wxStaticText *title = new wxStaticText(panel, wxID_ANY, "Select jobs to import");
    wxFont title_font = title->GetFont();
    title_font.SetPointSize(12);
    title_font.SetWeight(wxFONTWEIGHT_BOLD);
    title->SetFont(title_font);

    job_list_ = new wxCheckListBox(panel, wxID_ANY);
    job_list_->SetMinSize(wxSize(-1, 200));

    wxStaticBoxSizer *order_box =
        new wxStaticBoxSizer(wxVERTICAL, panel, "Import order");
    back_of_queue_ = new wxRadioButton(order_box->GetStaticBox(),
                                       wxID_ANY,
                                       "Back of queue",
                                       wxDefaultPosition,
                                       wxDefaultSize,
                                       wxRB_GROUP);
    wxRadioButton *front_of_queue =
        new wxRadioButton(order_box->GetStaticBox(), wxID_ANY, "Front of queue");
    back_of_queue_->SetValue(true);
    order_box->Add(back_of_queue_, 0, wxALL, 4);
    order_box->Add(front_of_queue, 0, wxALL, 4);

    content->Add(title, 0, wxBOTTOM, 8);
    content->Add(job_list_, 1, wxEXPAND | wxBOTTOM, 12);
    content->Add(order_box, 0, wxEXPAND | wxBOTTOM, 8);

    wxBoxSizer *button_row = new wxBoxSizer(wxHORIZONTAL);
    wxButton *cancel = new wxButton(panel, wxID_CANCEL, "Cancel");
    wxButton *import_button = new wxButton(panel, wxID_OK, "Import");
    import_button->SetDefault();
    button_row->AddStretchSpacer();
    button_row->Add(cancel, 0, wxRIGHT, 8);
    button_row->Add(import_button, 0);

    root->Add(content, 1, wxEXPAND | wxALL, 16);
    root->Add(new wxStaticLine(panel), 0, wxEXPAND | wxLEFT | wxRIGHT, 16);
    root->Add(button_row, 0, wxEXPAND | wxALL, 16);

    panel->SetSizer(root);

    Bind(wxEVT_BUTTON, &ImportDialog::OnImport, this, wxID_OK);

    PopulateCandidates();
}

void ImportDialog::PopulateCandidates() {
    candidates_ = import_watcher_.GetReadyImports();
    job_list_->Clear();
    for (const auto &candidate : candidates_) {
        job_list_->Append(candidate.display_name);
    }
}

void ImportDialog::OnImport(wxCommandEvent &event) {
    std::vector<wxString> selected_paths;
    const unsigned int count = job_list_->GetCount();
    for (unsigned int index = 0; index < count; ++index) {
        if (job_list_->IsChecked(index)) {
            selected_paths.push_back(candidates_[index].path);
        }
    }

    if (selected_paths.empty()) {
        wxMessageBox("Select at least one job to import.", "Import jobs", wxOK | wxICON_INFORMATION);
        return;
    }

    wxString error_message;
    if (!import_watcher_.ImportFiles(selected_paths, &error_message)) {
        wxMessageBox(error_message.empty() ? "Unable to import selected jobs."
                                           : error_message,
                     "Import failed",
                     wxOK | wxICON_ERROR);
        return;
    }

    EndModal(wxID_OK);
}

BambuQueueFrame::BambuQueueFrame(AppBootstrap &app_core)
    : wxFrame(nullptr, wxID_ANY, "Bambu Queue", wxDefaultPosition, wxSize(980, 640)),
      app_core_(app_core),
      import_timer_(this) {
    wxPanel *panel = new wxPanel(this);
    panel->SetBackgroundColour(wxColour("#F5F6F7"));

    wxBoxSizer *root = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *header = new wxBoxSizer(wxHORIZONTAL);

    wxStaticText *title = new wxStaticText(panel, wxID_ANY, "Queue");
    wxFont title_font = title->GetFont();
    title_font.SetPointSize(14);
    title_font.SetWeight(wxFONTWEIGHT_BOLD);
    title->SetFont(title_font);

    import_button_ = new wxButton(panel, wxID_ANY, "Import");
    import_button_->SetBackgroundColour(wxColour("#2FBF9B"));
    import_button_->SetForegroundColour(wxColour("#FFFFFF"));

    import_badge_ = new wxStaticText(panel, wxID_ANY, "");
    import_badge_->SetBackgroundColour(wxColour("#2FBF9B"));
    import_badge_->SetForegroundColour(wxColour("#FFFFFF"));
    import_badge_->SetMinSize(wxSize(24, 18));
    import_badge_->SetWindowStyleFlag(wxALIGN_CENTER_HORIZONTAL | wxALIGN_CENTER_VERTICAL);

    header->Add(title, 0, wxALIGN_CENTER_VERTICAL);
    header->AddStretchSpacer();
    header->Add(import_badge_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    header->Add(import_button_, 0, wxALIGN_CENTER_VERTICAL);

    wxStaticText *subtitle =
        new wxStaticText(panel, wxID_ANY, "Manage your queued print jobs.");
    subtitle->SetForegroundColour(wxColour("#6B7178"));

    wxPanel *queue_panel = new wxPanel(panel);
    queue_panel->SetBackgroundColour(*wxWHITE);

    wxListCtrl *queue_list = new wxListCtrl(queue_panel,
                                            wxID_ANY,
                                            wxDefaultPosition,
                                            wxDefaultSize,
                                            wxLC_REPORT | wxLC_SINGLE_SEL);
    queue_list->SetBackgroundColour(*wxWHITE);
    queue_list->InsertColumn(0, "Name");
    queue_list->InsertColumn(1, "Printer");
    queue_list->InsertColumn(2, "Time");
    queue_list->InsertColumn(3, "Filaments");
    queue_list->InsertColumn(4, "Plate");
    queue_list->InsertColumn(5, "Details");
    queue_list->SetColumnWidth(0, 200);
    queue_list->SetColumnWidth(1, 120);
    queue_list->SetColumnWidth(2, 90);
    queue_list->SetColumnWidth(3, 130);
    queue_list->SetColumnWidth(4, 100);
    queue_list->SetColumnWidth(5, 220);

    wxStaticText *empty_state =
        new wxStaticText(queue_panel, wxID_ANY, "No queued jobs yet");
    wxFont empty_font = empty_state->GetFont();
    empty_font.SetPointSize(11);
    empty_state->SetFont(empty_font);
    empty_state->SetForegroundColour(wxColour("#6B7178"));

    wxBoxSizer *queue_sizer = new wxBoxSizer(wxVERTICAL);
    queue_sizer->AddStretchSpacer();
    queue_sizer->Add(empty_state, 0, wxALIGN_CENTER_HORIZONTAL);
    queue_sizer->AddStretchSpacer();
    queue_panel->SetSizer(queue_sizer);

    const bool has_jobs = false;
    queue_list->Show(has_jobs);
    queue_panel->Show(!has_jobs);

    root->Add(header, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 16);
    root->Add(subtitle, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
    root->Add(queue_list, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 16);
    root->Add(queue_panel, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

    panel->SetSizer(root);

    Bind(wxEVT_BUTTON, &BambuQueueFrame::OnImportClicked, this, import_button_->GetId());
    Bind(wxEVT_TIMER, &BambuQueueFrame::OnImportTimer, this);
    import_timer_.Start(1000);
    UpdateImportBadge();
}

void BambuQueueFrame::OnImportClicked(wxCommandEvent &event) {
    wxUnusedVar(event);
    auto *import_watcher = app_core_.GetImportWatcher();
    if (!import_watcher) {
        wxMessageBox("Import service is unavailable.", "Import jobs", wxOK | wxICON_WARNING);
        return;
    }

    ImportDialog dialog(this, *import_watcher);
    if (dialog.ShowModal() == wxID_OK) {
        UpdateImportBadge();
    }
}

void BambuQueueFrame::OnImportTimer(wxTimerEvent &event) {
    wxUnusedVar(event);
    UpdateImportBadge();
}

void BambuQueueFrame::UpdateImportBadge() {
    auto *import_watcher = app_core_.GetImportWatcher();
    if (!import_watcher) {
        import_badge_->Hide();
        Layout();
        return;
    }

    const size_t ready_count = import_watcher->GetReadyImportCount();
    if (ready_count == 0) {
        import_badge_->Hide();
    } else {
        import_badge_->SetLabel(wxString::Format("%zu", ready_count));
        import_badge_->Show();
    }
    Layout();
}

wxIMPLEMENT_APP(BambuQueueApp);
