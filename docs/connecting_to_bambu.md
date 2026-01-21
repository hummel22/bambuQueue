# Connecting to a Bambu printer (LAN mode)

This app connects to Bambu printers over the local network using **MQTT + FTPS** in LAN
mode. You will need the printer **IP address**, **Access Code**, and **Serial**.

## 1) Enable LAN mode

On the printer touchscreen:

1. Open **Settings → Network**.
2. Enable **LAN Mode**.
3. Note the **IP address** and **Access Code** shown on screen.
4. Note the printer **Serial** (needed for MQTT topics).

> Tip: If you don’t see the serial on the network screen, it’s usually available in
> Settings → Device info.

## 2) Add the printer in BambuQueue

1. Launch the app.
2. Click **Add printer**.
3. Enter:
   - **Printer name** (anything friendly, e.g. `X1C-Lab`)
   - **Printer IP**
   - **Access code** (8-digit)
4. Save the printer.

The current UI does not prompt for the printer **Serial**, so add it manually in the
config file (see below) to enable MQTT topics like `device/<serial>/report`.

The app persists printer definitions in its config file:

```
~/Library/Application Support/BambuQueue/config.ini
```

Example printer entry:

```
[printers/0]
name=X1C-Lab
host=192.168.1.25
access_code=12345678
serial=01S00A0B000000
```

## 3) Required network access

Your Mac must reach the printer over the following ports:

| Service | Port | Notes |
| --- | --- | --- |
| MQTT (TLS) | 8883 | Username `bblp`, password = Access Code |
| FTPS | 990 | Upload `.3mf` / `.gcode` files |

## 4) How BambuQueue uses the connection

- **MQTT** is used for sending commands and listening to printer status updates.
- **FTPS** is used to upload print files before dispatching a job.

For protocol details and payload shapes, see `docs/api.md`.
