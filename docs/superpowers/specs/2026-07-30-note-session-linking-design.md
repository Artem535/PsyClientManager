# Note-Session Linking, Event Navigation, and Jump-to-Latest — Design

Issue: #57 (part 1 of 2 — the system event-log half is a separate, later spec)

## Goal

Close three of the four remaining gaps from the original chat-journal vision
that #48 and #30 didn't cover:

1. A note can be linked to the specific session it's about.
2. Clicking a session entry (or a note's session badge) in the timeline
   navigates to that event in the Calendar.
3. A jump-to-latest control appears when the feed is scrolled away from the
   bottom.

(The fourth piece — a system event-log for status/payment/reschedule
changes — needs its own schema design and is deliberately out of scope
here; tracked as the second half of #57.)

## Current state

- `ClientNote` (schema.hpp) has no session association at all.
- The merged feed (`ClientNotesPage::reloadNotes`, from #30) already
  distinguishes real vs. virtual sessions via `DuckEvent::is_virtual_occurrence`,
  `series_id`, and `original_occurrence_start` — virtual occurrences have
  synthetic negative IDs computed as
  `-(series_id * 1'000'000 + occurrence.date().toJulianDay())`
  (`qtimeline_model.cpp`, mirrored in `eventsForClient`). This ID is
  deterministic per occurrence date but is **replaced** by a real positive
  ID once that occurrence is materialized (confirmed/edited).
- Event selection today (`QEventView::eventSelected` →
  `QTimelineWidget::eventSelected` → `QEventInfoPage::onTimelineEventSelected`)
  is entirely internal to the Calendar page — nothing outside it can
  request "open this specific event."
- `QTimelineWidget`/`QEventInfoPage` already regenerate virtual IDs
  identically per day via `QTimelineModel::loadEventsForDay`, so a virtual
  ID computed elsewhere (e.g. in the Notes feed) will match the ID the
  Calendar page computes for the same day, as long as both sides use the
  same formula.

## Scope decisions (confirmed)

- **Link storage**: `ClientNote` gains three nullable columns —
  `linked_event_id` (for materialized/standalone sessions) OR
  `linked_series_id` + `linked_occurrence_start_ms` (for still-virtual
  recurring occurrences). Exactly one of the two forms is set at a time.
  Storing the series+occurrence-start pair (rather than the virtual event's
  synthetic ID directly) means the link survives materialization — the
  resolver below finds the *real* row once one exists, without needing a
  migration step when materialization happens.
- **Setting the link**: composer-time only, via a "Linked: <session>"
  button defaulting to the nearest *past* session (mirrors "provel vstrechu
  → zapisal nablyudenie"). Clicking it opens a small popup listing the
  ~8 nearest sessions (±3 months, same window `eventsForClient` already
  uses) plus "Don't link" — a `QListWidget` in a popup, not a `QComboBox`
  (the composer is built pre-`show()`, same reentrancy hazard fixed in
  #48/#30). Changing a note's link after it's saved is out of scope here —
  general note editing is #28.
- **Event navigation**: a new `openEventRequested(int64_t eventId, qint64
  dayMs)` signal, emitted by `ClientNotesPage` when a session card or a
  note's session badge is clicked. `MainWindow` switches to the Calendar
  tab and forwards it into `QEventInfoPage`, which sets the calendar's
  selected day (materializing that day's virtual IDs via the existing
  `loadEventsForDay` path) and then opens the event whose ID matches.
- **Jump-to-latest**: a small button anchored above the composer, shown
  when `mScrollArea->verticalScrollBar()->value() < maximum()`, hidden at
  the bottom; click scrolls to `maximum()`.
- **Testing**: DB round-trip test for the three new `ClientNote` columns
  (add + read back); backup/restore round-trip test confirming the link
  survives a backup/restore cycle; unit test(s) for the link-resolution
  helper (materialized event found by ID; virtual occurrence resolved by
  series+start; virtual occurrence that has since been materialized
  resolves to the materialized row, not a synthetic duplicate).
- **Out of scope**: changing a note's link after creation (#28's
  territory), cascading behavior when a linked event is deleted (the
  resolver simply returns nothing to link to — the badge doesn't render —
  no special-cased warning UI), the system event-log (second half of #57).

## Data model

```cpp
// schema.hpp — DuckClientNote additions
std::optional<std::int64_t> linked_event_id = std::nullopt;
std::optional<std::int64_t> linked_series_id = std::nullopt;
std::optional<std::int64_t> linked_occurrence_start_ms = std::nullopt;
```

Migration (appended to `kSchemaMigrations`):

```sql
ALTER TABLE ClientNote ADD COLUMN IF NOT EXISTS linked_event_id INTEGER;
ALTER TABLE ClientNote ADD COLUMN IF NOT EXISTS linked_series_id INTEGER;
ALTER TABLE ClientNote ADD COLUMN IF NOT EXISTS linked_occurrence_start TIMESTAMP;
```

`kInsertClientNoteQuery` gains three more bound parameters for these
columns; `kSelectClientNotesQuery`'s explicit column list gains the three
new columns (in the same order the `DuckClientNote(chunk, index)`
constructor reads positionally).

## Link resolution

New free function alongside `eventsForClient` in `recurrence_utils.h/.cpp`
(same module — it composes the same materialized/virtual building blocks):

```cpp
std::optional<DuckEvent> resolveNoteLink(pcm::database::Database &db,
                                         const DuckClientNote &note);
```

Implementation:
- If `linked_event_id` is set: `db.get_event(*linked_event_id)`. If the
  underlying series occurrence was since materialized under a *different*
  row (shouldn't normally happen for this branch — materialization reuses
  the original virtual identity only via the series+start branch below),
  this is a direct, cheap lookup.
- Else if `linked_series_id` is set: check
  `db.get_materialized_occurrence_starts_for_series(*linked_series_id)`
  for `*linked_occurrence_start_ms`; if present, resolve the real
  materialized event (existing `get_events_for_client`-style lookup,
  filtered by series+start); otherwise rebuild the virtual `DuckEvent` via
  `pcm::recurrence::occurrences` + `buildVirtualOccurrence` for that single
  occurrence date, matching the same synthetic ID formula used elsewhere.
- Else: `std::nullopt`.

## UI

### Composer link picker

- New `QPushButton *mLinkSessionButton` in the composer row, labeled
  `tr("Linked: %1").arg(...)` or `tr("Link to a session")` when unset.
- On click, opens a small popup (`QMenu` hosting a `QListWidget`, or a
  borderless `QWidget` positioned under the button — implementation detail
  for the plan) listing the nearest ~8 sessions from the already-fetched
  feed events (no extra DB round trip — `reloadNotes` already has them),
  sorted by `|start_date - now|`, plus a "Don't link" row.
- Selecting a session stores its identity (real `id`, or
  `series_id`/`original_occurrence_start` if virtual) into pending
  composer state, applied when `onAddNoteClicked` builds the `DuckClientNote`.
- Default selection (shown in the button before the user touches it):
  nearest **past** session from the same list, if any.

### Note bubble session badge

- `addNoteBubble` calls `resolveNoteLink`; if resolved, renders a small
  clickable label (e.g. `🔗 12.05.2026 15:00`) below the timestamp, above
  the body. Clicking it emits `openEventRequested(event.id, dayStartMs)`
  the same way a session card does.

### Session card / badge → Calendar navigation

- `ClientNotesPage` gains `signal void openEventRequested(int64_t eventId,
  qint64 dayMs)`, emitted from `addSessionEntry`'s click handler and the
  note badge's click handler alike.
- `MainWindow::connectSignals()` connects it to a new private slot that:
  switches to `Pages::calendar` (mirroring the existing
  `showPage(...)` pattern used for `openClientCardRequested`), then calls
  a new `QEventInfoPage::openEventOnDay(int64_t eventId, qint64 dayMs)`.
- `QEventInfoPage::openEventOnDay` sets the calendar widget's selected date
  from `dayMs` (driving the existing `RoundedCalendarWidget::clicked` →
  `QTimelineWidget::onSelectedDayChanged` path so the day's events —
  including virtual occurrences — load), then finds the event with a
  matching ID in `mTimelineWidget`'s freshly loaded model and opens it via
  the existing `onTimelineEventSelected`-equivalent path.

### Jump-to-latest button

- New `QPushButton *mJumpToLatestButton` layered above the composer
  surface (or as the last row inside it), initially hidden.
- Connected to `mScrollArea->verticalScrollBar()`'s `valueChanged`/
  `rangeChanged` signals: visible when `value() < maximum()`, hidden
  otherwise. Click sets `value()` to `maximum()`.

## Testing

- DB round-trip: add a note with `linked_event_id` set, read back, verify
  round-trips; same for the `linked_series_id`/`linked_occurrence_start_ms`
  pair.
- Backup/restore round-trip: extend the existing backup/restore test
  coverage to confirm a linked note's link fields survive a backup →
  restore cycle.
- `resolveNoteLink` unit/DB-backed tests: materialized-event link resolves
  directly; virtual-occurrence link (not yet materialized) resolves to a
  rebuilt virtual `DuckEvent` with the expected synthetic ID; virtual link
  whose occurrence has since been materialized resolves to the real
  materialized row (no duplicate).
- No dedicated UI test for the composer popup, badge rendering, or
  jump-to-latest button — consistent with this codebase's existing lack of
  UI test coverage for `ClientNotesPage`; verified by clean build and
  manual smoke test, as done for #48/#30.

## Out of scope (deferred)

- Changing a note's session link after the note is saved (#28).
- Cascading UI when a linked event/series is later deleted.
- The system event-log for status/payment/reschedule changes (second half
  of #57, separate spec).
