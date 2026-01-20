#include "app/ThreeMfImporter.h"

#include <wx/filename.h>
#include <wx/log.h>
#include <wx/regex.h>
#include <wx/sstream.h>
#include <wx/wfstream.h>
#include <wx/xml/xml.h>
#include <wx/zipstrm.h>

#include <algorithm>
#include <unordered_map>

namespace {
bool IsThumbnailEntry(const wxString &entry_name) {
    const wxString lower = entry_name.Lower();
    return lower.EndsWith("thumbnail.png") || lower.EndsWith("thumbnail.jpg") ||
           lower.EndsWith("thumbnail.jpeg");
}

bool IsMetadataEntry(const wxString &entry_name) {
    return entry_name.Lower().EndsWith("metadata.xml");
}

bool IsGcodeEntry(const wxString &entry_name) {
    return entry_name.Lower().EndsWith(".gcode");
}

wxString NormalizeMetadataName(const wxString &name) {
    wxString normalized = name.Lower();
    normalized.Replace(" ", "");
    normalized.Replace("_", "");
    normalized.Replace("-", "");
    return normalized;
}
}  // namespace

ThreeMfImporter::ThreeMfImporter(const AppConfig &config, DatabaseManager &database)
    : config_(config), database_(database) {}

bool ThreeMfImporter::ImportFile(const wxString &file_path, wxString *error_message) {
    if (file_path.empty()) {
        if (error_message) {
            *error_message = "Missing 3MF import path.";
        }
        return false;
    }

    if (database_.JobExistsForFile(file_path)) {
        return true;
    }

    wxString thumbnail_entry;
    PrintMetadata metadata;
    std::vector<PlateDefinition> plates;
    if (!Extract3mfData(file_path, &thumbnail_entry, &metadata, &plates, error_message)) {
        return false;
    }

    const wxFileName source_file(file_path);
    const wxString base_name = source_file.GetName();
    wxString target_file_path =
        ResolveUniquePath(config_.jobs_dir, base_name, ".gcode.3mf");

    if (!wxRenameFile(file_path, target_file_path, true)) {
        if (error_message) {
            *error_message = wxString::Format("Unable to move imported file to %s",
                                              target_file_path);
        }
        wxLogWarning("ThreeMfImporter: failed to move %s to %s",
                     file_path,
                     target_file_path);
        return false;
    }

    wxString thumbnail_path;
    if (!thumbnail_entry.empty()) {
        thumbnail_path =
            ResolveUniquePath(config_.jobs_dir, base_name + "_thumb", ".png");
        if (!ExtractThumbnailEntry(target_file_path,
                                   thumbnail_entry,
                                   thumbnail_path,
                                   error_message)) {
            wxLogWarning("ThreeMfImporter: thumbnail extraction failed for %s",
                         target_file_path);
            thumbnail_path.clear();
        }
    }

    if (plates.empty()) {
        plates.push_back({1, "Plate 1"});
    }

    const wxString metadata_json = BuildMetadataJson(metadata);
    for (const auto &plate : plates) {
        const wxString job_name = wxString::Format("%s - %s",
                                                   base_name,
                                                   plate.name.empty()
                                                       ? wxString::Format("Plate %d",
                                                                          plate.plate_index)
                                                       : plate.name);
        std::vector<PlateDefinition> job_plate = {plate};
        int job_id = 0;
        if (!database_.InsertImportedJob(job_name,
                                         target_file_path,
                                         thumbnail_path,
                                         metadata_json,
                                         job_plate,
                                         &job_id,
                                         error_message)) {
            return false;
        }
    }

    wxLogMessage("ThreeMfImporter: imported %s with %zu plate(s)",
                 target_file_path,
                 plates.size());
    return true;
}

bool ThreeMfImporter::Extract3mfData(const wxString &file_path,
                                     wxString *thumbnail_source,
                                     PrintMetadata *metadata,
                                     std::vector<PlateDefinition> *plates,
                                     wxString *error_message) {
    wxFileInputStream file_stream(file_path);
    if (!file_stream.IsOk()) {
        if (error_message) {
            *error_message = "Unable to open 3MF file.";
        }
        wxLogWarning("ThreeMfImporter: unable to open %s", file_path);
        return false;
    }

    wxZipInputStream zip_stream(file_stream);
    std::unique_ptr<wxZipEntry> entry;
    std::vector<wxString> gcode_entries;
    wxString metadata_entry;
    wxString thumb_entry;

    while ((entry.reset(zip_stream.GetNextEntry())), entry) {
        if (entry->IsDir()) {
            zip_stream.CloseEntry();
            continue;
        }

        const wxString entry_name = entry->GetName();
        if (thumb_entry.empty() && IsThumbnailEntry(entry_name)) {
            thumb_entry = entry_name;
        }
        if (metadata_entry.empty() && IsMetadataEntry(entry_name)) {
            metadata_entry = entry_name;
        }
        if (IsGcodeEntry(entry_name)) {
            gcode_entries.push_back(entry_name);
        }
        zip_stream.CloseEntry();
    }

    if (!metadata_entry.empty() && metadata) {
        if (!ReadMetadataEntry(file_path, metadata_entry, metadata, error_message)) {
            wxLogWarning("ThreeMfImporter: failed to read metadata %s from %s",
                         metadata_entry,
                         file_path);
        }
    }

    if (thumbnail_source) {
        *thumbnail_source = thumb_entry;
    }

    if (plates) {
        PopulatePlatesFromEntries(gcode_entries, plates);
    }

    return true;
}

bool ThreeMfImporter::ExtractThumbnailEntry(const wxString &file_path,
                                            const wxString &entry_name,
                                            const wxString &destination_path,
                                            wxString *error_message) {
    wxFileInputStream file_stream(file_path);
    if (!file_stream.IsOk()) {
        if (error_message) {
            *error_message = "Unable to open 3MF file for thumbnail.";
        }
        return false;
    }

    wxZipInputStream zip_stream(file_stream);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zip_stream.GetNextEntry())), entry) {
        if (entry->IsDir()) {
            zip_stream.CloseEntry();
            continue;
        }
        if (entry->GetName() != entry_name) {
            zip_stream.CloseEntry();
            continue;
        }

        wxFileOutputStream output(destination_path);
        if (!output.IsOk()) {
            if (error_message) {
                *error_message = "Unable to write thumbnail file.";
            }
            return false;
        }
        output.Write(zip_stream);
        zip_stream.CloseEntry();
        return output.IsOk();
    }

    if (error_message) {
        *error_message = "Thumbnail entry not found in 3MF.";
    }
    return false;
}

bool ThreeMfImporter::ReadMetadataEntry(const wxString &file_path,
                                        const wxString &entry_name,
                                        PrintMetadata *metadata,
                                        wxString *error_message) {
    wxFileInputStream file_stream(file_path);
    if (!file_stream.IsOk()) {
        if (error_message) {
            *error_message = "Unable to open 3MF file for metadata.";
        }
        return false;
    }

    wxZipInputStream zip_stream(file_stream);
    std::unique_ptr<wxZipEntry> entry;
    while ((entry.reset(zip_stream.GetNextEntry())), entry) {
        if (entry->IsDir()) {
            zip_stream.CloseEntry();
            continue;
        }
        if (entry->GetName() != entry_name) {
            zip_stream.CloseEntry();
            continue;
        }

        wxString xml_text;
        wxStringOutputStream output(&xml_text);
        output.Write(zip_stream);
        zip_stream.CloseEntry();
        if (xml_text.empty()) {
            return false;
        }
        return ParseMetadataXml(xml_text, metadata);
    }

    if (error_message) {
        *error_message = "Metadata entry not found in 3MF.";
    }
    return false;
}

bool ThreeMfImporter::ParseMetadataXml(const wxString &xml_text, PrintMetadata *metadata) {
    if (!metadata) {
        return false;
    }

    wxStringInputStream input(xml_text);
    wxXmlDocument doc;
    if (!doc.Load(input)) {
        return false;
    }

    wxXmlNode *root = doc.GetRoot();
    if (!root) {
        return false;
    }

    for (wxXmlNode *node = root->GetChildren(); node; node = node->GetNext()) {
        if (node->GetName().Lower() != "metadata") {
            continue;
        }
        const wxString name_attr = node->GetAttribute("name", "");
        const wxString normalized = NormalizeMetadataName(name_attr);
        const wxString value = node->GetNodeContent().Trim().Trim(false);

        if (metadata->estimated_time.empty() &&
            normalized.Contains("time") &&
            (normalized.Contains("estimate") || normalized.Contains("estimated"))) {
            metadata->estimated_time = value;
            continue;
        }
        if (metadata->estimated_length.empty() &&
            (normalized.Contains("length") || normalized.Contains("filament"))) {
            metadata->estimated_length = value;
            continue;
        }
        if (metadata->material_usage.empty() &&
            (normalized.Contains("material") || normalized.Contains("usage") ||
             normalized.Contains("weight"))) {
            metadata->material_usage = value;
            continue;
        }
    }

    return true;
}

wxString ThreeMfImporter::BuildMetadataJson(const PrintMetadata &metadata) const {
    wxString json = "{";
    bool first = true;
    auto append_field = [&](const wxString &key, const wxString &value) {
        if (value.empty()) {
            return;
        }
        if (!first) {
            json += ", ";
        }
        json += wxString::Format("\"%s\":\"%s\"", key, EscapeJson(value));
        first = false;
    };

    append_field("estimated_time", metadata.estimated_time);
    append_field("estimated_length", metadata.estimated_length);
    append_field("material_usage", metadata.material_usage);

    json += "}";
    return first ? wxString() : json;
}

wxString ThreeMfImporter::EscapeJson(const wxString &value) const {
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

wxString ThreeMfImporter::ResolveUniquePath(const wxString &directory,
                                            const wxString &base_name,
                                            const wxString &extension) const {
    wxString candidate =
        wxFileName(directory, base_name + extension).GetFullPath();
    if (!wxFileExists(candidate)) {
        return candidate;
    }

    int counter = 1;
    while (true) {
        wxString name = wxString::Format("%s-%d", base_name, counter);
        candidate = wxFileName(directory, name + extension).GetFullPath();
        if (!wxFileExists(candidate)) {
            return candidate;
        }
        counter++;
    }
}

void ThreeMfImporter::PopulatePlatesFromEntries(const std::vector<wxString> &entries,
                                                std::vector<PlateDefinition> *plates) const {
    if (!plates) {
        return;
    }

    std::unordered_map<int, wxString> plate_map;
    int fallback_index = 1;
    for (const auto &entry : entries) {
        wxFileName file(entry);
        const wxString lower = file.GetName().Lower();
        int plate_index = 0;
        wxString plate_name;

        wxRegEx regex("plate[_ -]?([0-9]+)", wxRE_ICASE);
        if (regex.Matches(lower)) {
            wxString match = regex.GetMatch(lower, 1);
            long parsed = 0;
            if (match.ToLong(&parsed) && parsed > 0) {
                plate_index = static_cast<int>(parsed);
            }
        }

        if (plate_index == 0) {
            plate_index = fallback_index;
        }
        fallback_index++;
        plate_name = wxString::Format("Plate %d", plate_index);
        plate_map[plate_index] = plate_name;
    }

    plates->clear();
    for (const auto &item : plate_map) {
        plates->push_back({item.first, item.second});
    }

    std::sort(plates->begin(),
              plates->end(),
              [](const PlateDefinition &a, const PlateDefinition &b) {
                  return a.plate_index < b.plate_index;
              });
}
