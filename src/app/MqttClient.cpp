#include "app/MqttClient.h"

#include <wx/log.h>
#include <wx/txtstrm.h>
#include <wx/utils.h>

MqttClient::MqttClient() = default;

MqttClient::~MqttClient() {
    Stop();
}

bool MqttClient::Publish(const wxString &host,
                         const wxString &access_code,
                         const wxString &topic,
                         const wxString &payload,
                         wxString *error_message) {
    if (host.empty() || access_code.empty() || topic.empty()) {
        if (error_message) {
            *error_message = "MQTT publish failed: missing host, access code, or topic.";
        }
        wxLogError("MqttClient: publish missing host/access/topic.");
        return false;
    }

    wxArrayString args;
    args.Add("mosquitto_pub");
    args.Add("-h");
    args.Add(host);
    args.Add("-p");
    args.Add("8883");
    args.Add("--tls-version");
    args.Add("tlsv1.2");
    args.Add("--insecure");
    args.Add("-u");
    args.Add("bblp");
    args.Add("-P");
    args.Add(access_code);
    args.Add("-t");
    args.Add(topic);
    args.Add("-m");
    args.Add(payload);

    const long result = wxExecute(args, wxEXEC_SYNC);
    if (result != 0) {
        if (error_message) {
            *error_message = "MQTT publish failed: mosquitto_pub exited with error.";
        }
        wxLogError("MqttClient: mosquitto_pub exited with code %ld", result);
        return false;
    }

    wxLogMessage("MqttClient: published to %s", topic);
    return true;
}

bool MqttClient::Subscribe(const wxString &host,
                           const wxString &access_code,
                           const wxString &topic,
                           MessageHandler handler,
                           wxString *error_message) {
    Stop();

    if (host.empty() || access_code.empty() || topic.empty()) {
        if (error_message) {
            *error_message = "MQTT subscribe failed: missing host, access code, or topic.";
        }
        wxLogError("MqttClient: subscribe missing host/access/topic.");
        return false;
    }

    process_ = std::make_unique<wxProcess>();
    process_->Redirect();

    wxArrayString args;
    args.Add("mosquitto_sub");
    args.Add("-h");
    args.Add(host);
    args.Add("-p");
    args.Add("8883");
    args.Add("--tls-version");
    args.Add("tlsv1.2");
    args.Add("--insecure");
    args.Add("-u");
    args.Add("bblp");
    args.Add("-P");
    args.Add(access_code);
    args.Add("-t");
    args.Add(topic);
    args.Add("-v");

    const long pid = wxExecute(args, wxEXEC_ASYNC, process_.get());
    if (pid == 0) {
        if (error_message) {
            *error_message = "MQTT subscribe failed: unable to start mosquitto_sub.";
        }
        wxLogError("MqttClient: mosquitto_sub failed to start.");
        process_.reset();
        return false;
    }

    stop_ = false;
    reader_thread_ = std::thread(&MqttClient::ReaderLoop, this, std::move(handler));
    wxLogMessage("MqttClient: subscribed to %s (pid %ld)", topic, pid);
    return true;
}

void MqttClient::Stop() {
    stop_ = true;
    if (process_) {
        wxProcess::Kill(process_->GetPid(), wxSIGTERM, wxKILL_CHILDREN);
    }
    if (reader_thread_.joinable()) {
        reader_thread_.join();
    }
    process_.reset();
}

void MqttClient::ReaderLoop(MessageHandler handler) {
    if (!process_ || !process_->IsInputOpened()) {
        return;
    }

    wxInputStream *stream = process_->GetInputStream();
    wxTextInputStream text(*stream);

    while (!stop_) {
        if (!stream->CanRead()) {
            wxMilliSleep(100);
            continue;
        }

        wxString line = text.ReadLine();
        if (line.empty()) {
            continue;
        }

        const int split = line.Find(' ');
        if (split == wxNOT_FOUND) {
            continue;
        }

        wxString topic = line.SubString(0, split - 1);
        wxString payload = line.SubString(split + 1, line.length() - 1);
        handler(topic, payload);
    }
}
