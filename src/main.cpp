#include <wx/checklst.h>
#include <wx/listctrl.h>
#include <wx/statline.h>
#include <wx/wx.h>

#include "app/AppBootstrap.h"

#include <algorithm>
#include <memory>
#include <numeric>
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
    struct FilamentInfo {
        wxString color;
        wxString material;
    };

    struct QueueItem {
        wxString name;
        wxString subtext;
        wxString printer;
        wxString printer_status;
        wxString time;
        wxString details;
        std::vector<FilamentInfo> filaments;
    };

    struct CompletedItem {
        wxString name;
        wxString printer;
        wxString time;
        wxString details;
        wxDateTime started_at;
        std::vector<FilamentInfo> filaments;
    };

    void OnImportClicked(wxCommandEvent &event);
    void OnImportTimer(wxTimerEvent &event);
    void UpdateImportBadge();
    void PopulateQueueList();
    void PopulateCompletedList();
    void OnQueueBeginDrag(wxListEvent &event);
    void OnQueueLeftUp(wxMouseEvent &event);
    void OnQueueContextMenu(wxContextMenuEvent &event);
    void OnQueueLeftDown(wxMouseEvent &event);
    void OnCompletedFilterChanged(wxCommandEvent &event);
    void OnQueueActionPrintNext(long item_index);
    wxString FormatFilaments(const std::vector<FilamentInfo> &filaments) const;
    wxString FormatPrinterStatus(const QueueItem &item) const;
    bool IsDragHandleClick(long item_index, const wxPoint &position) const;
    void ReorderQueueItems(long from_index, long to_index);

    AppBootstrap &app_core_;
    wxButton *import_button_ = nullptr;
    wxStaticText *import_badge_ = nullptr;
    wxTimer import_timer_;
    wxListCtrl *queue_list_ = nullptr;
    wxListCtrl *completed_list_ = nullptr;
    wxChoice *completed_filter_ = nullptr;
    wxImageList *plate_images_ = nullptr;
    std::vector<QueueItem> queue_items_;
    std::vector<CompletedItem> completed_items_;
    long drag_index_ = -1;
    bool drag_started_on_handle_ = false;
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

    wxBoxSizer *filter_row = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText *completed_label = new wxStaticText(panel, wxID_ANY, "Completed");
    completed_label->SetForegroundColour(wxColour("#6B7178"));
    completed_filter_ = new wxChoice(panel, wxID_ANY);
    completed_filter_->Append("Last day");
    completed_filter_->Append("Last week");
    completed_filter_->Append("Last year");
    completed_filter_->SetSelection(1);
    filter_row->AddStretchSpacer();
    filter_row->Add(completed_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    filter_row->Add(completed_filter_, 0, wxALIGN_CENTER_VERTICAL);

    queue_list_ = new wxListCtrl(panel,
                                 wxID_ANY,
                                 wxDefaultPosition,
                                 wxDefaultSize,
                                 wxLC_REPORT | wxLC_SINGLE_SEL);
    queue_list_->SetBackgroundColour(*wxWHITE);
    queue_list_->InsertColumn(0, "");
    queue_list_->InsertColumn(1, "Name");
    queue_list_->InsertColumn(2, "Printer");
    queue_list_->InsertColumn(3, "Time");
    queue_list_->InsertColumn(4, "Filaments");
    queue_list_->InsertColumn(5, "Plate");
    queue_list_->InsertColumn(6, "Details");
    queue_list_->InsertColumn(7, "Actions");
    queue_list_->SetColumnWidth(0, 32);
    queue_list_->SetColumnWidth(1, 200);
    queue_list_->SetColumnWidth(2, 150);
    queue_list_->SetColumnWidth(3, 90);
    queue_list_->SetColumnWidth(4, 140);
    queue_list_->SetColumnWidth(5, 80);
    queue_list_->SetColumnWidth(6, 220);
    queue_list_->SetColumnWidth(7, 110);

    completed_list_ = new wxListCtrl(panel,
                                     wxID_ANY,
                                     wxDefaultPosition,
                                     wxDefaultSize,
                                     wxLC_REPORT | wxLC_SINGLE_SEL);
    completed_list_->SetBackgroundColour(*wxWHITE);
    completed_list_->InsertColumn(0, "Name");
    completed_list_->InsertColumn(1, "Printer");
    completed_list_->InsertColumn(2, "Started");
    completed_list_->InsertColumn(3, "Filaments");
    completed_list_->InsertColumn(4, "Plate");
    completed_list_->InsertColumn(5, "Details");
    completed_list_->SetColumnWidth(0, 200);
    completed_list_->SetColumnWidth(1, 150);
    completed_list_->SetColumnWidth(2, 120);
    completed_list_->SetColumnWidth(3, 140);
    completed_list_->SetColumnWidth(4, 80);
    completed_list_->SetColumnWidth(5, 260);

    plate_images_ = new wxImageList(56, 56, true);
    wxBitmap plate_bitmap(56, 56);
    {
        wxMemoryDC dc(plate_bitmap);
        dc.SetBackground(wxBrush(wxColour("#F0F1F3")));
        dc.Clear();
        dc.SetPen(wxPen(wxColour("#D1D5DB"), 1));
        dc.SetBrush(wxBrush(wxColour("#F8FAFC")));
        dc.DrawRoundedRectangle(4, 4, 48, 48, 6);
        dc.SetPen(wxPen(wxColour("#A0A7AF"), 2));
        dc.DrawCircle(wxPoint(28, 28), 12);
        dc.DrawLine(16, 28, 40, 28);
        dc.DrawLine(28, 16, 28, 40);
    }
    plate_images_->Add(plate_bitmap);
    queue_list_->AssignImageList(plate_images_, wxIMAGE_LIST_SMALL);
    completed_list_->AssignImageList(plate_images_, wxIMAGE_LIST_SMALL);

    root->Add(header, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 16);
    root->Add(subtitle, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
    root->Add(filter_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
    root->Add(queue_list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);
    root->Add(new wxStaticLine(panel), 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 16);
    wxStaticText *completed_title = new wxStaticText(panel, wxID_ANY, "Completed jobs");
    wxFont completed_font = completed_title->GetFont();
    completed_font.SetPointSize(12);
    completed_font.SetWeight(wxFONTWEIGHT_BOLD);
    completed_title->SetFont(completed_font);
    root->Add(completed_title, 0, wxLEFT | wxRIGHT | wxTOP, 16);
    root->Add(completed_list_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

    panel->SetSizer(root);

    Bind(wxEVT_BUTTON, &BambuQueueFrame::OnImportClicked, this, import_button_->GetId());
    Bind(wxEVT_TIMER, &BambuQueueFrame::OnImportTimer, this);
    queue_list_->Bind(wxEVT_LIST_BEGIN_DRAG, &BambuQueueFrame::OnQueueBeginDrag, this);
    queue_list_->Bind(wxEVT_LEFT_UP, &BambuQueueFrame::OnQueueLeftUp, this);
    queue_list_->Bind(wxEVT_LEFT_DOWN, &BambuQueueFrame::OnQueueLeftDown, this);
    queue_list_->Bind(wxEVT_CONTEXT_MENU, &BambuQueueFrame::OnQueueContextMenu, this);
    completed_filter_->Bind(wxEVT_CHOICE, &BambuQueueFrame::OnCompletedFilterChanged, this);
    import_timer_.Start(1000);
    UpdateImportBadge();
    PopulateQueueList();
    PopulateCompletedList();
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

void BambuQueueFrame::PopulateQueueList() {
    queue_items_ = {
        {"Dockside Tool Tray", "job-10421", "P1S-Office", "Idle", "2h 10m", "0.16mm • 0.4mm nozzle",
         {{"#E11D48", "PLA"}, {"#0EA5E9", "PETG"}}},
        {"Hydroponic Mount", "job-10422", "X1C-Lab", "Printing", "45m", "0.2mm • Draft profile",
         {{"#22C55E", "PLA"}, {"#F97316", "PLA"}, {"#111827", "ABS"}, {"#9333EA", "PLA"},}},
        {"Panel Clips", "job-10425", "A1-Mini", "Error", "1h 5m", "0.12mm • Fine profile",
         {{"#FACC15", "PETG"}}},
    };

    queue_list_->DeleteAllItems();
    for (size_t index = 0; index < queue_items_.size(); ++index) {
        const auto &item = queue_items_[index];
        long row = queue_list_->InsertItem(static_cast<long>(index), "⋮⋮");
        queue_list_->SetItem(row, 1, item.name);
        queue_list_->SetItem(row, 2, FormatPrinterStatus(item));
        queue_list_->SetItem(row, 3, item.time);
        queue_list_->SetItem(row, 4, FormatFilaments(item.filaments));
        queue_list_->SetItem(row, 5, "");
        queue_list_->SetItemColumnImage(row, 5, 0);
        queue_list_->SetItem(row, 6, item.details);
        queue_list_->SetItem(row, 7, "Print next");
        if (!item.subtext.empty()) {
            queue_list_->SetItemTextColour(row, wxColour("#1E1F22"));
        }
        if (index % 2 == 0) {
            queue_list_->SetItemBackgroundColour(row, wxColour("#F8FAFC"));
        }
    }
}

void BambuQueueFrame::PopulateCompletedList() {
    completed_items_.clear();
    completed_items_.push_back({"Display Bracket",
                                "X1C-Lab",
                                "1h 40m",
                                "0.2mm • Standard profile",
                                wxDateTime::Now() - wxTimeSpan::Hours(8),
                                {{"#0EA5E9", "PLA"}}});
    completed_items_.push_back({"Gear Housing",
                                "P1S-Office",
                                "3h 10m",
                                "0.16mm • 0.4mm nozzle",
                                wxDateTime::Now() - wxTimeSpan::Days(3),
                                {{"#22C55E", "PETG"}, {"#111827", "ABS"}}});
    completed_items_.push_back({"Cable Clip Set",
                                "A1-Mini",
                                "25m",
                                "0.2mm • Draft profile",
                                wxDateTime::Now() - wxTimeSpan::Days(20),
                                {{"#F97316", "PLA"}}});

    wxTimeSpan filter_span;
    switch (completed_filter_->GetSelection()) {
        case 0:
            filter_span = wxTimeSpan::Days(1);
            break;
        case 2:
            filter_span = wxTimeSpan::Days(365);
            break;
        default:
            filter_span = wxTimeSpan::Days(7);
            break;
    }
    wxDateTime cutoff = wxDateTime::Now() - filter_span;

    std::vector<size_t> indices(completed_items_.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [&](size_t a, size_t b) {
        return completed_items_[a].started_at.IsLaterThan(completed_items_[b].started_at);
    });

    completed_list_->DeleteAllItems();
    long row = 0;
    for (size_t index : indices) {
        const auto &item = completed_items_[index];
        if (item.started_at.IsEarlierThan(cutoff)) {
            continue;
        }
        long new_row = completed_list_->InsertItem(row, item.name);
        completed_list_->SetItem(new_row, 1, item.printer);
        completed_list_->SetItem(new_row, 2, item.started_at.FormatISODate());
        completed_list_->SetItem(new_row, 3, FormatFilaments(item.filaments));
        completed_list_->SetItem(new_row, 4, "");
        completed_list_->SetItemColumnImage(new_row, 4, 0);
        completed_list_->SetItem(new_row, 5, item.details);
        if (row % 2 == 0) {
            completed_list_->SetItemBackgroundColour(new_row, wxColour("#F8FAFC"));
        }
        row++;
    }
}

void BambuQueueFrame::OnQueueBeginDrag(wxListEvent &event) {
    const long item_index = event.GetIndex();
    if (!IsDragHandleClick(item_index, event.GetPoint())) {
        return;
    }
    drag_index_ = item_index;
    drag_started_on_handle_ = true;
}

void BambuQueueFrame::OnQueueLeftDown(wxMouseEvent &event) {
    long item_index = -1;
    queue_list_->HitTest(event.GetPosition(), item_index);
    if (item_index != -1) {
        wxRect action_rect;
        if (queue_list_->GetSubItemRect(item_index, 7, action_rect) &&
            action_rect.Contains(event.GetPosition())) {
            OnQueueActionPrintNext(item_index);
            return;
        }
    }
    drag_started_on_handle_ = (item_index != -1) && IsDragHandleClick(item_index, event.GetPosition());
    event.Skip();
}

void BambuQueueFrame::OnQueueLeftUp(wxMouseEvent &event) {
    if (!drag_started_on_handle_ || drag_index_ == -1) {
        event.Skip();
        return;
    }

    long drop_index = -1;
    queue_list_->HitTest(event.GetPosition(), drop_index);
    if (drop_index != -1 && drop_index != drag_index_) {
        ReorderQueueItems(drag_index_, drop_index);
    }
    drag_index_ = -1;
    drag_started_on_handle_ = false;
    event.Skip();
}

void BambuQueueFrame::OnQueueContextMenu(wxContextMenuEvent &event) {
    wxPoint position = event.GetPosition();
    if (position == wxDefaultPosition) {
        position = queue_list_->GetClientAreaOrigin();
    } else {
        position = queue_list_->ScreenToClient(position);
    }
    long item_index = -1;
    queue_list_->HitTest(position, item_index);
    if (item_index == -1) {
        return;
    }

    wxMenu menu;
    menu.Append(1001, "Print now");
    menu.Append(1002, "Send to printer");
    menu.AppendSeparator();
    menu.Append(1003, "Clear");
    menu.Bind(
        wxEVT_MENU,
        [this](wxCommandEvent &event) {
            wxString action;
            switch (event.GetId()) {
                case 1001:
                    action = "Print now";
                    break;
                case 1002:
                    action = "Send to printer";
                    break;
                case 1003:
                    action = "Clear";
                    break;
                default:
                    action = "Action";
            }
            wxMessageBox(action + " selected.", "Queue actions", wxOK | wxICON_INFORMATION);
        },
        1001,
        1003);
    PopupMenu(&menu, position);
}

void BambuQueueFrame::OnCompletedFilterChanged(wxCommandEvent &event) {
    wxUnusedVar(event);
    PopulateCompletedList();
}

void BambuQueueFrame::OnQueueActionPrintNext(long item_index) {
    if (item_index < 0 || static_cast<size_t>(item_index) >= queue_items_.size()) {
        return;
    }
    wxMessageBox("Print next queued for " + queue_items_[item_index].name + ".",
                 "Queue action",
                 wxOK | wxICON_INFORMATION);
}

wxString BambuQueueFrame::FormatFilaments(const std::vector<FilamentInfo> &filaments) const {
    const size_t max_chips = 4;
    wxString text;
    for (size_t i = 0; i < filaments.size() && i < max_chips; ++i) {
        if (!text.empty()) {
            text += " • ";
        }
        text += filaments[i].material;
    }
    if (filaments.size() > max_chips) {
        text += wxString::Format(" +%zu", filaments.size() - max_chips);
    }
    return text;
}

wxString BambuQueueFrame::FormatPrinterStatus(const QueueItem &item) const {
    return item.printer + " (" + item.printer_status + ")";
}

bool BambuQueueFrame::IsDragHandleClick(long item_index, const wxPoint &position) const {
    if (item_index < 0) {
        return false;
    }
    wxRect rect;
    if (!queue_list_->GetSubItemRect(item_index, 0, rect)) {
        return false;
    }
    return rect.Contains(position);
}

void BambuQueueFrame::ReorderQueueItems(long from_index, long to_index) {
    if (from_index < 0 || to_index < 0) {
        return;
    }
    if (from_index == to_index) {
        return;
    }
    const size_t from = static_cast<size_t>(from_index);
    const size_t to = static_cast<size_t>(to_index);
    if (from >= queue_items_.size() || to >= queue_items_.size()) {
        return;
    }
    QueueItem moved = queue_items_[from];
    queue_items_.erase(queue_items_.begin() + static_cast<long>(from));
    queue_items_.insert(queue_items_.begin() + static_cast<long>(to), moved);
    PopulateQueueList();
}

wxIMPLEMENT_APP(BambuQueueApp);
