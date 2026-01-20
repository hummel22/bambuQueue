# Main Queue List View UI Spec

## Purpose
Define the layout, behavior, and visual style for the main queue list view so it mirrors the look and feel of Bambu Studio while supporting queue management actions.

## Layout Overview
- **Container**: Single primary view with a header row, filter/sort controls, and a scrollable list/table body.
- **Primary layout**: Table/grid with fixed header, zebra rows, and optional column reordering.
- **Row height**: Compact, consistent with Bambu Studio list density (target 36–44px for normal rows; 52–60px for rows with plate thumbnails).

## Columns (Left → Right)
1. **Name**
   - Primary job label, single line with ellipsis.
   - Optional subtext line (smaller, muted): file name or queue ID.
2. **Printer**
   - Printer name and status (idle/printing/error) in smaller text.
   - Status pill or dot indicator aligned left.
3. **Time**
   - Estimated print time or time remaining.
   - Format: `HH:MM` or `1h 20m`.
4. **Filaments**
   - Display filament color chips + material labels (e.g., PLA, PETG).
   - Max 4 chips inline, overflow as `+N`.
5. **Plate Image**
   - Thumbnail (square, 56–64px). 
   - If no preview, show a neutral placeholder with plate icon.
6. **Print Details**
   - Key info: layer height, nozzle size, profile name, or material profile.
   - Secondary, muted text; can wrap to two lines if needed.

## Sorting & Reordering
- **Sorting**
  - Column headers are clickable to sort ascending/descending.
  - Default sort: queue priority (if present), then created time ascending.
  - Visual indicator: arrow caret icon in header with subtle animation.
- **Reordering**
  - Drag-and-drop rows to reprioritize.
  - Drag handle appears on hover at the far left of each row ("grip" icon).
  - During drag: row lifts with shadow; target insertion line in accent color.

## Filters
- **Completed filter**
  - Toggle or segmented control labeled `Completed` with options:
    - `Last day`
    - `Last week`
    - `Last year`
  - Filter sits above the table, right-aligned with search/sort controls.

## Actions
- **Row action: Print next**
  - Inline button in row on hover or via action column if space allows.
  - Primary accent style (filled) when hovered; otherwise outlined.
- **Context menu** (right-click or three-dot icon)
  - `Print now`
  - `Send to printer`
  - `Clear`
  - Menu uses the same rounded corners and shadow as Bambu Studio.
- **Bulk actions**
  - Optional selection checkboxes on the left for multi-row actions.

## Visual Style (Bambu Studio-inspired)
- **Typography**
  - Use a modern sans-serif (Inter/Roboto-like) with medium weight headers.
  - Header text: 12–13px, uppercase or small-caps optional.
  - Body text: 13–14px, muted secondary text at 11–12px.
- **Color**
  - Background: cool neutral gray (#F5F6F7) with white table surface.
  - Primary text: near-black (#1E1F22).
  - Secondary text: muted gray (#6B7178).
  - Accent: Bambu-style teal/green (#2FBF9B) for highlights/active states.
  - Row hover: light teal tint (#E9F7F2).
- **Borders & elevation**
  - Subtle 1px dividers (#E1E4E8) between rows.
  - Header bottom border slightly stronger than body separators.
  - Context menu shadow: soft, large blur; radius 8–12px.
- **Icons**
  - Use thin-line (1.5–2px stroke) icons consistent with Bambu Studio.
  - Required icons: sort caret, grip/drag handle, printer, status dot, plate placeholder, three-dot menu.
  - Icons should be monochrome and inherit text color unless active.

## Empty & Loading States
- **Empty state**
  - Centered illustration of a plate with text: "No queued jobs yet".
  - Provide CTA to add a job if applicable.
- **Loading state**
  - Skeleton rows matching column layout.
  - Subtle shimmer in light gray.

## Accessibility
- Ensure keyboard navigation for sorting, row focus, drag-and-drop (optional alternative controls).
- Contrast ratio for text > 4.5:1 in normal state.
- Provide tooltips for truncated text and icon-only buttons.
