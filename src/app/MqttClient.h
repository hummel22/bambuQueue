#pragma once

#include <wx/process.h>
#include <wx/string.h>

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

class MqttClient {
public:
    using MessageHandler = std::function<void(const wxString &topic, const wxString &payload)>;

    MqttClient();
    ~MqttClient();

    bool Publish(const wxString &host,
                 const wxString &access_code,
                 const wxString &topic,
                 const wxString &payload,
                 wxString *error_message);
    bool Subscribe(const wxString &host,
                   const wxString &access_code,
                   const wxString &topic,
                   MessageHandler handler,
                   wxString *error_message);
    void Stop();

private:
    void ReaderLoop(MessageHandler handler);

    std::unique_ptr<wxProcess> process_;
    std::thread reader_thread_;
    std::atomic<bool> stop_{false};
};
