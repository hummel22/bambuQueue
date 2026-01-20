# Implementation Plan (Refined)

This plan reflects the latest product decisions:

- **Export type**: `*.gcode.3mf`
- **Queue**: global queue (single list)
- **Default import position**: back of queue
- **Filament validation**: match **material + color**
- **Persistence**: **SQLite**
- **Completed list**: read-only, chronological by **job started time**
- **UI style**: follow Bambu Studio palette/fonts; open-source icons if needed

## Phased PR plan

### PR 1 — App scaffolding + config + SQLite
- Add core application layer and configuration system.
- Initialize data paths: `/jobs`, `/completed`, monitored import folder.
- Add SQLite database for jobs, printers, and settings.
- Add logging and error surface for missing/invalid config.

### PR 2 — Job model + persistence layer
- Implement job schema in SQLite (job, plate, filament, printer, status).
- Store per-job metadata and thumbnail path references.
- Implement status transitions and file moves between `/jobs` and `/completed`.
- Enforce completed ordering by **job started timestamp**.

### PR 3 — Import pipeline (gcode.3mf)
- Watch the import directory for new `*.gcode.3mf` files.
- Extract per-plate metadata + thumbnails (one job per plate).
- Create job records as **Imported**.
- Provide import dialog to select jobs and set import order, defaulting to **back of queue**.

### PR 4 — Main queue UI (wxWidgets)
- Implement list view per `docs/ui_spec.md` (name, printer, time, filaments, thumbnail, details).
- Add drag-and-drop reordering for queued jobs.
- Add completed filter (last day/week/year), read-only list sorted by **job started time**.
- Add row actions: Print next, context menu (print now, send to printer, clear).

### PR 5 — Printer integration (LAN mode)
- Implement MQTT + FTPS client based on `docs/api.md`.
- Upload `*.gcode.3mf` via FTPS and issue MQTT `project_file` command.
- Subscribe to status updates and update job state to **Printing/Completed**.

### PR 6 — Preflight validation
- Validate printer compatibility before dispatch.
- Match filament **material + color** against AMS trays.
- Block dispatch if the printer is busy or AMS mismatch is detected.

### PR 7 — UX polish + onboarding
- Add onboarding for printer IP + access code.
- Surface validation errors and queue status.
- Add empty states, loading states, and user tips.

## Open questions to finalize before PR 3–4

1. **Metadata extraction scope**: which fields must be shown in “Print Details” from `*.gcode.3mf`?
2. **Thumbnail extraction**: is the standard 3MF preview sufficient or do we need per-plate renders?
3. **Queue ordering rules**: do “Imported” jobs appear in the main list or only once queued?

