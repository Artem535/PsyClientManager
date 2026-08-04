# Notes screen → chat-like client journal (lean first pass) — Design

Issue: #48

## Goal

Evolve the existing `ClientNotesPage` from a plain note feed into a
chronological, chat-like client journal: date-grouped entries, a clear
client-context header, compact attachments, a faster composer, and a basic
feed filter. This is explicitly a **lean first pass** — session-linking
("Встречи" entries), system-event lines (schedule/payment changes), a
real last/next-appointment header summary, and in-place note editing are
all deferred to #26/#28/#30, which provide the backend prerequisites
(per-client event queries, note revision/soft-delete). Attempting those
here now would duplicate work those issues will do properly.

## Current state

`ClientNotesPage` (`src/pages/client_notes_page/`) is already a
single-column vertical feed, not left/right chat bubbles — `addNoteBubble`
always left-aligns. It renders markdown via `QTextBrowser`, shows inline
image previews at a fixed max size, and has a composer (`QPlainTextEdit`
+ "Attach files" + "Add note" buttons) at the bottom. Notes are fetched via
`Database::get_client_notes(client_id)`, already ordered
`ORDER BY created_at ASC, id ASC`. Attachments are fetched per-note via
`Database::get_note_attachments(note_id)`.

There is no `update_client_note`, no soft delete, no `event_id` on
`DuckClientNote`, and no `Database` method to fetch events for a given
client (events are only queried by day/range or by id). Building
session-linking or editing now would require adding all of that — out of
scope for this pass.

## Architecture

Pure UI/presentation change to `ClientNotesPage`. No schema changes, no
new `Database` methods except one new cross-page **signal** (not a DB
call) for client-card navigation. Everything is built from existing reads
(`get_client_notes`, `get_note_attachments`) and existing writes
(`add_client_note`, `add_client_note_attachment`).

## Components

### 1. Client-card navigation signal

The client list's per-row "Notes" action (`ClientInfo::notesButtonClicked`
in `client_info.cpp`) jumps straight to `ClientNotesPage` without loading
`QClientInfoCardPage` first — so a "back to client card" link from Notes
currently has nothing to jump to.

Add `void ClientNotesPage::openClientCardRequested(const DuckClient &client);`,
emitted by a new header button. Wire it in `main_window.cpp`'s
`connectSignals()`:

```cpp
connect(clientNotesPage, &ClientNotesPage::openClientCardRequested,
        clientCardPage, &QClientInfoCardPage::setClientInfo);
connect(clientNotesPage, &ClientNotesPage::openClientCardRequested,
        [this]() {
          setClientNavigationVisible(Pages::clientCard, true);
          showPage(Pages::clientCard, mBtnProfile);
        });
```

This mirrors the existing `displayButtonClicked` wiring exactly.

### 2. Client header (breadcrumb)

Replace the current stacked "Notes" title + client-name label with a
single breadcrumb-style header: `<Client name> → Заметки`, plus a small
link/button "Открыть карточку клиента" next to it, emitting
`openClientCardRequested`. No appointment summary in this pass (needs
#26/#30-level per-client event queries).

### 3. Date-grouped feed with dividers

While iterating notes in `reloadNotes` (already ascending by
`created_at`), track the previous note's local calendar date. When it
changes, insert a small centered divider widget before that note's bubble:
a thin horizontal rule with a muted, centered date label (e.g.
`— 24 июля 2026 —`), visually distinct from note bubbles (no background,
no border). Single linear pass, no new sorting/query logic.

### 4. Compact attachments, expand on click

Replace "always render full-size image inline" in `addAttachmentWidgets`.
Every attachment — image or file — renders as one compact row: type
icon/label, filename, human-readable size, and an "Открыть" button
(unchanged behavior — opens externally via `QDesktopServices`). For
images specifically, clicking the row (not the "Открыть" button) toggles
an inline scaled preview open/closed below the row, reusing the existing
`QImageReader`/`QPixmap` scaling logic. Collapsed by default.

### 5. Composer polish

- `Ctrl+Return` / `Ctrl+Enter` in the composer triggers the same code
  path as clicking "Add note" — implemented via an event filter on the
  `QPlainTextEdit` checking `Qt::ControlModifier` with `Key_Return` or
  `Key_Enter`.
- After a successful save, show a small transient label near the composer
  ("Заметка сохранена"), auto-hidden after ~2s via
  `QTimer::singleShot(2000, ...)`. No persisted draft state — notes save
  instantly today, so there's nothing to protect against loss.

### 6. Feed filter: Все / С вложениями

A small `QComboBox` above the feed with two entries: "Все" and
"С вложениями". When "С вложениями" is active, `reloadNotes` skips notes
whose `get_note_attachments(note.id)` result is empty — reuses the
attachment fetch that already happens per-note when building each bubble,
so no extra query is introduced.

## Error handling

No new failure modes are introduced — every code path reuses existing
`Database` calls whose error handling (return empty vector / `<= 0` on
failure) is already in place and already handled by the current UI
(empty-state label, disabled controls when no client is selected).

## Testing

No dedicated unit test — `ClientNotesPage` has no existing test coverage
and is not linked into any test target, consistent with the
`meeting_utils`/`confirmation_utils` precedent for pure-UI Qt widgets with
no test-target linkage. Verified by a clean build; the underlying
`Database` methods this relies on (`get_client_notes`,
`get_note_attachments`, `add_client_note`, `add_client_note_attachment`)
are already covered by existing DB tests and are unmodified here.

## Out of scope (deferred)

- Linking a note to a specific calendar event / session ("Встречи" filter
  option) — needs `event_id` on `DuckClientNote` and a per-client event
  query; tracked under #26/#30.
- System-event lines ("Встреча назначена", "Статус оплаты изменён", etc.)
  interleaved in the feed — needs the same per-client event aggregation as
  #30 (`ClientTimeline`).
- Last/next-appointment summary in the header — same dependency.
- In-place note editing and soft delete — needs `update_client_note` and
  revision/soft-delete columns; tracked under #28.
- Cross-client search.
