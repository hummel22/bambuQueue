#pragma once

#include "app/AppConfig.h"
#include "app/DatabaseManager.h"
#include "app/FtpsClient.h"
#include "app/MqttClient.h"

#include <memory>
#include <map>

class PrinterCoordinator {
public:
    PrinterCoordinator(const AppConfig &config, DatabaseManager &database);
    ~PrinterCoordinator();

    bool Start(wxString *error_message);

private:
    struct PrinterSession {
        PrinterDefinition definition;
        int printer_id = 0;
        bool is_printing = false;
        MqttClient mqtt;
    };

    void HandleReport(PrinterSession &printer, const wxString &payload);
    bool DispatchNextJob(PrinterSession &printer);

    const AppConfig &config_;
    DatabaseManager &database_;
    FtpsClient ftps_client_;
    std::map<wxString, PrinterSession> sessions_;
};
