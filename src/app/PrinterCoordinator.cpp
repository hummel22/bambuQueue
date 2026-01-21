#include "app/PrinterCoordinator.h"

#include <wx/filename.h>
#include <wx/log.h>

#include <cctype>
#include <optional>

namespace {
wxString EscapeJsonString(const wxString &value) {
    wxString escaped;
    escaped.reserve(value.size());
    for (const wxUniChar ch : value) {
        if (ch == '\\') {
            escaped += "\\\\";
        } else if (ch == '"') {
            escaped += "\\\"";
        } else if (ch == '\n') {
            escaped += "\\n";
        } else if (ch == '\r') {
            escaped += "\\r";
        } else if (ch == '\t') {
            escaped += "\\t";
        } else {
            escaped += ch;
        }
    }
    return escaped;
}

std::optional<wxString> ExtractJsonString(const wxString &payload, const wxString &key) {
    const std::string data = payload.ToStdString();
    const std::string needle = "\"" + key.ToStdString() + "\"";
    size_t pos = data.find(needle);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    pos = data.find(':', pos);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    ++pos;
    while (pos < data.size() && std::isspace(static_cast<unsigned char>(data[pos]))) {
        ++pos;
    }
    if (pos >= data.size() || data[pos] != '"') {
        return std::nullopt;
    }
    ++pos;
    std::string value;
    while (pos < data.size()) {
        char ch = data[pos];
        if (ch == '"') {
            break;
        }
        if (ch == '\\' && pos + 1 < data.size()) {
            char next = data[pos + 1];
            if (next == '"' || next == '\\') {
                value += next;
                pos += 2;
                continue;
            }
        }
        value += ch;
        ++pos;
    }
    return wxString::FromUTF8(value);
}

std::optional<int> ExtractJsonInt(const wxString &payload, const wxString &key) {
    const std::string data = payload.ToStdString();
    const std::string needle = "\"" + key.ToStdString() + "\"";
    size_t pos = data.find(needle);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    pos = data.find(':', pos);
    if (pos == std::string::npos) {
        return std::nullopt;
    }
    ++pos;
    while (pos < data.size() && std::isspace(static_cast<unsigned char>(data[pos]))) {
        ++pos;
    }
    std::string number;
    while (pos < data.size() &&
           (std::isdigit(static_cast<unsigned char>(data[pos])) || data[pos] == '-' ||
            data[pos] == '.')) {
        number += data[pos];
        ++pos;
    }
    try {
        if (!number.empty()) {
            return static_cast<int>(std::stod(number));
        }
    } catch (const std::exception &) {
    }
    return std::nullopt;
}

wxString PrinterKey(const PrinterDefinition &printer) {
    return printer.name.empty() ? printer.host : printer.name;
}

wxString BuildProjectFilePayload(const wxString &remote_file, int plate_index) {
    const wxString plate_path =
        wxString::Format("Metadata/plate_%d.gcode", plate_index <= 0 ? 1 : plate_index);
    return wxString::Format(
        "{"
        "\"print\":{"
        "\"command\":\"project_file\","
        "\"param\":\"%s\","
        "\"file\":\"%s\","
        "\"url\":\"ftp:///%s\","
        "\"bed_leveling\":true,"
        "\"flow_cali\":true,"
        "\"vibration_cali\":true,"
        "\"layer_inspect\":false,"
        "\"sequence_id\":\"10000000\""
        "}"
        "}",
        EscapeJsonString(plate_path),
        EscapeJsonString(remote_file),
        EscapeJsonString(remote_file));
}

bool IsPrintingState(const wxString &state) {
    const wxString lowered = state.Lower();
    return lowered.Contains("print") || lowered.Contains("run") || lowered.Contains("busy");
}

bool IsCompletedState(const wxString &state) {
    const wxString lowered = state.Lower();
    return lowered.Contains("finish") || lowered.Contains("complete") || lowered.Contains("idle");
}
}  // namespace

PrinterCoordinator::PrinterCoordinator(const AppConfig &config, DatabaseManager &database)
    : config_(config), database_(database) {}

PrinterCoordinator::~PrinterCoordinator() {
    for (auto &entry : sessions_) {
        entry.second.mqtt.Stop();
    }
}

bool PrinterCoordinator::Start(wxString *error_message) {
    if (config_.printers.empty()) {
        return true;
    }

    std::map<wxString, int> printer_ids;
    if (!database_.EnsurePrinters(config_.printers, &printer_ids, error_message)) {
        return false;
    }

    for (const auto &printer : config_.printers) {
        if (printer.host.empty() || printer.access_code.empty() || printer.serial.empty()) {
            wxLogWarning("PrinterCoordinator: skipping printer with missing host/access/serial.");
            continue;
        }

        const wxString key = PrinterKey(printer);
        auto [it_session, inserted] = sessions_.emplace(key, PrinterSession{});
        PrinterSession &session = it_session->second;
        session.definition = printer;
        auto it = printer_ids.find(key);
        if (it != printer_ids.end()) {
            session.printer_id = it->second;
        }

        const wxString report_topic =
            wxString::Format("device/%s/report", printer.serial);
        wxString subscribe_error;
        if (!session.mqtt.Subscribe(
                printer.host,
                printer.access_code,
                report_topic,
                [this, key](const wxString &topic, const wxString &payload) {
                    wxUnusedVar(topic);
                    auto it_session = sessions_.find(key);
                    if (it_session == sessions_.end()) {
                        return;
                    }
                    HandleReport(it_session->second, payload);
                },
                &subscribe_error)) {
            wxLogWarning("PrinterCoordinator: failed to subscribe to %s: %s",
                         report_topic,
                         subscribe_error);
        }

        DispatchNextJob(session);
    }

    return true;
}

void PrinterCoordinator::HandleReport(PrinterSession &printer, const wxString &payload) {
    const auto gcode_state = ExtractJsonString(payload, "gcode_state");
    const auto gcode_file = ExtractJsonString(payload, "gcode_file");
    const auto percent = ExtractJsonInt(payload, "mc_percent");

    if (!gcode_state || !gcode_file) {
        return;
    }

    const wxFileName file_name(*gcode_file);
    int job_id = 0;
    if (!database_.FindActiveJobByFileName(file_name.GetFullName(),
                                           printer.printer_id,
                                           &job_id,
                                           nullptr) ||
        job_id == 0) {
        return;
    }

    if (IsPrintingState(*gcode_state)) {
        if (database_.UpdateJobStatus(job_id, "printing", config_.jobs_dir, config_.completed_dir,
                                      nullptr)) {
            printer.is_printing = true;
        }
        return;
    }

    if (IsCompletedState(*gcode_state) && percent.value_or(100) >= 99) {
        if (database_.UpdateJobStatus(job_id, "completed", config_.jobs_dir, config_.completed_dir,
                                      nullptr)) {
            printer.is_printing = false;
            DispatchNextJob(printer);
        }
    }
}

bool PrinterCoordinator::DispatchNextJob(PrinterSession &printer) {
    if (printer.is_printing) {
        return true;
    }

    QueuedJob job;
    if (!database_.GetNextQueuedJob(printer.printer_id, &job, nullptr)) {
        return false;
    }
    if (job.id == 0) {
        return true;
    }

    const wxFileName local_file(job.file_path);
    const wxString remote_name = local_file.GetFullName();
    wxString upload_error;
    if (!ftps_client_.UploadFile(printer.definition.host,
                                 printer.definition.access_code,
                                 job.file_path,
                                 remote_name,
                                 &upload_error)) {
        wxLogWarning("PrinterCoordinator: FTPS upload failed: %s", upload_error);
        return false;
    }

    const wxString payload = BuildProjectFilePayload(remote_name, job.plate_index);
    const wxString command_topic =
        wxString::Format("device/%s/request", printer.definition.serial);
    wxString publish_error;
    if (!printer.mqtt.Publish(printer.definition.host,
                              printer.definition.access_code,
                              command_topic,
                              payload,
                              &publish_error)) {
        wxLogWarning("PrinterCoordinator: MQTT publish failed: %s", publish_error);
        return false;
    }

    database_.AssignJobToPrinter(job.id, printer.printer_id, nullptr);
    database_.UpdateJobStatus(job.id, "printing", config_.jobs_dir, config_.completed_dir, nullptr);
    printer.is_printing = true;
    wxLogMessage("PrinterCoordinator: dispatched job %d to %s", job.id, printer.definition.name);
    return true;
}
