# Local Bambu Studio / Printer API Notes

This document summarizes the **local** interfaces that Bambu Studio (and other
local tools) use to communicate with Bambu Lab printers in LAN mode. It is based
on official LAN-mode guidance plus community reverse-engineering work and
open-source client implementations.

> **Status:** Local APIs are unofficial and can change with firmware updates.
> Always validate against a real printer/firmware before shipping production
> automation.

## LAN mode prerequisites

1. **Enable LAN mode** on the printer, then read the **Access Code** and **IP**
   address from the printer UI. This access code is the password used for local
   MQTT/FTPS/camera connections.
2. Bambu Studio and local tools connect directly to the printer IP over the
   local network.

Official reference:
- Bambu Lab wiki: *How to enable LAN Mode on Bambu Lab printers* (access code,
  LAN mode workflow). <https://wiki.bambulab.com/en/knowledge-sharing/enable-lan-mode>

## Host / port discovery

There is **no published service discovery protocol** in the official docs, so
for the queue app we should assume **manual IP + access code** entry:

- **Primary discovery method:** user supplies the printer IP and access code
  (shown on the printer while LAN mode is enabled).
- **Optional convenience:** provide an “IP scan” or mDNS helper if we decide to
  implement it later, but it should remain optional because it is not documented
  as a stable protocol.

## Local API surfaces (LAN mode)

The community API clients (example: `bambulabs_api`) show three primary local
interfaces used by Bambu printers:

| Surface | Port | Auth | Purpose |
| --- | --- | --- | --- |
| MQTT (TLS) | **8883** | username `bblp`, password **Access Code** | Commands + status updates |
| FTPS (implicit TLS) | **990** | username `bblp`, password **Access Code** | Upload/inspect print files |
| Camera stream (TLS socket) | **6000** | username `bblp`, password **Access Code** | JPEG frames for live view |

Community implementation references:
- MQTT client default port, username, and TLS config: <https://github.com/BambuTools/bambulabs_api/blob/main/bambulabs_api/mqtt_client.py>
- FTPS client default port + credentials: <https://github.com/BambuTools/bambulabs_api/blob/main/bambulabs_api/ftp_client.py>
- Camera client port + auth handshake: <https://github.com/BambuTools/bambulabs_api/blob/main/bambulabs_api/camera_client.py>

## MQTT endpoints (topics) and payload shapes

**Topics** (per printer serial):

- **Command topic:** `device/<serial>/request`
- **Report topic:** `device/<serial>/report`

The printer publishes status on the report topic. Clients publish JSON commands
on the request topic.

### Common request payloads

All payloads are JSON objects with a top-level key matching a subsystem. Known
examples:

**Request a full “pushall” state update**
```json
{
  "pushing": { "command": "pushall" }
}
```

**Request firmware info / history**
```json
{
  "info": { "command": "get_version" },
  "upgrade": { "command": "get_history" }
}
```

**Start a print from a project file (3MF / GCode on FTPS)**
```json
{
  "print": {
    "command": "project_file",
    "param": "Metadata/plate_1.gcode",
    "file": "job.3mf",
    "bed_leveling": true,
    "bed_type": "textured_plate",
    "flow_cali": true,
    "vibration_cali": true,
    "url": "ftp:///job.3mf",
    "layer_inspect": false,
    "sequence_id": "10000000",
    "use_ams": true,
    "ams_mapping": [0],
    "skip_objects": null
  }
}
```

**Toggle printer light**
```json
{ "system": { "led_mode": "on" } }
```

**Enable/disable onboard timelapse**
```json
{ "camera": { "command": "ipcam_record_set", "control": "enable" } }
```

These shapes (and fields) are taken directly from community code used to drive
real printers. See the references in the MQTT client implementation.

### Common report fields (status responses)

The printer’s report payload is merged into a state document that includes
modules like `print`, `info`, and `upgrade`. The MQTT client in
`bambulabs_api` reads the following fields from the `print` report:

- `print.mc_percent` — percentage complete
- `print.mc_remaining_time` — remaining time (seconds)
- `print.gcode_state` — printer state
- `print.gcode_file` — current/last file name
- `print.spd_mag` — print speed multiplier
- `print.lights_report[0].mode` — light state

These keys are used to interpret the printer’s live status and can inform queue
logic (idle vs printing, progress, etc.).

## FTPS endpoints (file upload)

The printer exposes **implicit FTPS on port 990**. The queue app should upload
print files (e.g., `.3mf`, `.gcode`) before sending a `project_file` MQTT
command.

Notes:
- Username: `bblp`
- Password: Access Code
- Examples: list directories (`LIST`), upload (`STOR`), download (`RETR`).

## Camera stream (optional)

A TLS socket on **port 6000** provides JPEG frames. The community client uses a
binary handshake containing username `bblp` and the access code, then reads
frame headers + JPEG payloads.

## C++ client libraries

The queue app can be implemented in C++ with the following libraries:

**HTTP / networking**
- **Boost.Asio / Boost.Beast** (low-level TCP/TLS + MQTT via a library like
  `mqtt_cpp`)
- **POCO Net** (TLS sockets, FTP, HTTP)
- **cpp-httplib** (simple HTTP client/server, not MQTT)

**MQTT**
- **Eclipse Paho MQTT C/C++** (well-supported)
- **mqtt_cpp** (header-only, works with Boost.Asio)

**JSON**
- **nlohmann/json** (ergonomic, widely used)
- **RapidJSON** (fast, DOM/SAX)
- **simdjson** (very fast, strict requirements)

> Note: Bambu’s local API uses MQTT + FTPS + raw TLS sockets, so HTTP libraries
> alone are insufficient; ensure you pick an MQTT and FTPS-capable stack.

## Queue app compatibility checks (printer + filament)

Before dispatching a job, the queue app should verify the printer and filament
are compatible with the job. Suggested workflow:

1. **Identify printer model + firmware**
   - Read the `info` report (e.g., `get_version`) to confirm firmware and
     printer model information.
2. **Validate nozzle + hardware constraints**
   - Track the printer’s nozzle diameter and type in inventory data and verify
     the job’s required nozzle size and material constraints. Community code
     enumerates nozzle types and supported diameters for validation.
3. **Verify AMS / filament availability**
   - Parse the `ams` report to read tray metadata. Each tray includes
     `tray_type`, `tray_info_idx`, and temperature ranges.
   - Match the job’s filament requirement to an available tray (by material
     type and temperature range), and ensure AMS mapping is configured.
4. **Temperature range safety**
   - Ensure the job’s nozzle/bed temperature targets fall within the tray’s
     `nozzle_temp_min`/`nozzle_temp_max` and any printer constraints.
5. **Fail fast** if no tray matches, the printer is busy, or the firmware/model
   mismatch suggests the job was sliced for a different machine.

Community data structures useful for implementing this check:
- `PrinterType`/`NozzleType` and known nozzle diameters for printer constraints.
- `FilamentTray` fields like `tray_type`, `tray_info_idx`,
  `nozzle_temp_min`, and `nozzle_temp_max`.
- `Filament` settings for known materials and temperature ranges.

## References

- **Official:** Bambu Lab Wiki — LAN Mode setup
  <https://wiki.bambulab.com/en/knowledge-sharing/enable-lan-mode>
- **Community:** BambuTools `bambulabs_api` MQTT client (topics + payloads)
  <https://github.com/BambuTools/bambulabs_api/blob/main/bambulabs_api/mqtt_client.py>
- **Community:** BambuTools `bambulabs_api` FTPS client (port 990, access code)
  <https://github.com/BambuTools/bambulabs_api/blob/main/bambulabs_api/ftp_client.py>
- **Community:** BambuTools `bambulabs_api` Camera client (port 6000, auth)
  <https://github.com/BambuTools/bambulabs_api/blob/main/bambulabs_api/camera_client.py>
- **Community:** BambuTools filament/printer enums and tray data structures
  <https://github.com/BambuTools/bambulabs_api/blob/main/bambulabs_api/filament_info.py>
  <https://github.com/BambuTools/bambulabs_api/blob/main/bambulabs_api/printer_info.py>
  <https://github.com/BambuTools/bambulabs_api/blob/main/bambulabs_api/ams.py>
