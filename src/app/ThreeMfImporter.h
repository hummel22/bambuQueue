#pragma once

#include "app/AppConfig.h"
#include "app/DatabaseManager.h"

#include <wx/string.h>

#include <vector>

struct PrintMetadata {
    wxString estimated_time;
    wxString estimated_length;
    wxString material_usage;
};

class ThreeMfImporter {
public:
    ThreeMfImporter(const AppConfig &config, DatabaseManager &database);

    bool ImportFile(const wxString &file_path, wxString *error_message);

private:
    bool Extract3mfData(const wxString &file_path,
                        wxString *thumbnail_source,
                        PrintMetadata *metadata,
                        std::vector<PlateDefinition> *plates,
                        wxString *error_message);
    bool ExtractThumbnailEntry(const wxString &file_path,
                               const wxString &entry_name,
                               const wxString &destination_path,
                               wxString *error_message);
    bool ReadMetadataEntry(const wxString &file_path,
                           const wxString &entry_name,
                           PrintMetadata *metadata,
                           wxString *error_message);
    bool ParseMetadataXml(const wxString &xml_text, PrintMetadata *metadata);
    wxString BuildMetadataJson(const PrintMetadata &metadata) const;
    wxString EscapeJson(const wxString &value) const;
    wxString ResolveUniquePath(const wxString &directory,
                               const wxString &base_name,
                               const wxString &extension) const;
    void PopulatePlatesFromEntries(const std::vector<wxString> &entries,
                                   std::vector<PlateDefinition> *plates) const;

    const AppConfig &config_;
    DatabaseManager &database_;
};
