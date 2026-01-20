# Queue Model

## Job entity

Each job represents one printable plate with its associated slice output and assets.

**Required fields**

- **Job ID**: Unique identifier for the job (immutable once created).
- **Plate ID**: Identifier for the plate inside the source project/slice.
- **Printer**: Target printer model or identifier the job was sliced for.
- **Filament**: Filament profile(s) used for the slice (e.g., material + color per slot).
- **Slice metadata**: Key slice outputs (layer height, estimated time, material usage, etc.).
- **Thumbnail**: Preview image for the plate/job (path or blob reference).
- **Status**: Current lifecycle state (see states below).

**Optional/common fields**

- **Source file**: Original project/plate file name or path.
- **Imported at**: Timestamp of when the job was created from import.
- **Queued at / Started at / Completed at**: Timestamps for state transitions.
- **Notes**: User-supplied info.

## States

Jobs move through a simple lifecycle:

1. **Imported**: Discovered and parsed from an incoming file, but not yet queued.
2. **Queued**: Ready to print or awaiting a printer assignment.
3. **Printing**: Actively printing on a selected printer.
4. **Completed**: Finished, cleared, or otherwise removed from active printing/queue.

## Transitions

- **Imported → Queued**: User action to enqueue (e.g., “Queue” button) or auto-queue policy.
- **Queued → Printing**: User starts the job on a specific printer.
- **Printing → Completed**: Print completes successfully or is marked finished.
- **Queued → Completed**: Clear/cancel action removes a queued job from active list.
- **Printing → Completed**: Clear button for a printing job marks it completed immediately.

> **Clear button behavior:** The clear action always moves the job to **Completed**, regardless of whether it is currently **Queued** or **Printing**.

## Folder behaviors

The application relies on two primary folders for storage and lifecycle management:

### `/jobs`

- Contains active job files that are **Imported**, **Queued**, or **Printing**.
- Incoming files from the monitored directory are copied/moved here after successful import.
- Each job’s artifacts (metadata, thumbnail, slice output) should be stored alongside or within a per-job subfolder.

### `/completed`

- Contains finalized jobs in the **Completed** state.
- When a job is cleared or finishes printing, its job folder and artifacts move from `/jobs` to `/completed`.
- Completed jobs are read-only for display/history purposes.

## Import flow (monitored directory)

1. **Monitor** a configured input directory for new slice/project files.
2. **On detection**:
   - Validate the file type and parse slice metadata.
   - Extract plate(s), thumbnails, and per-plate metadata.
3. **Create jobs**:
   - For each plate, generate a Job ID and populate the job entity fields.
   - Set initial status to **Imported**.
4. **Persist**:
   - Write job metadata and assets into `/jobs`.
   - Optionally archive or remove the original source file from the monitored directory.
5. **Queueing**:
   - Jobs remain **Imported** until a user queues them or an auto-queue policy applies.

## Notes for implementation

- Status is the single source of truth for list placement (active vs completed).
- The UI should reflect the status transitions and update in real time.
- Folder moves should be atomic where possible to prevent partially-moved jobs.
