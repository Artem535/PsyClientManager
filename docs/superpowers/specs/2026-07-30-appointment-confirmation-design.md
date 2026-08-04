# Appointment Confirmation Tracking — Design

Issue: #21 — "Add appointment confirmation tracking"

## Goal

Let a psychologist track whether a client has confirmed an upcoming session, independently of the event's `event_stat_id`/`payment_stat_id`.

## Scope

- Confirmation state on events (confirmed/unconfirmed), independent of `event_stat_id`.
- Visual indicator for confirmation state on Timeline event cards.
- A way to filter/surface unconfirmed upcoming sessions on the Timeline.
- Quick action to mark a session confirmed (and unconfirm again), and copy a "please confirm" message using a configurable template.

## Out of scope

- Actually sending the confirmation request (SMS/Telegram/email) — this only tracks state and prepares message copy (mirrors how `meetingInviteTemplate` already only prepares copy, not sends anything).
- Changing or removing the existing `event_stat_id = 4` ("confirmed") status value — that is a separate, pre-existing concept (an internal scheduling status, mutually exclusive with "scheduled" in the same combo box) that already participates in the reminder-eligibility filter (`event_stat_id IN (1, 2, 4)`). This issue adds an orthogonal field; the old status value is left untouched.
- A cross-day "unconfirmed sessions" overview list — that belongs to the separate, already-tracked roadmap issue #27 ("daily task/overview list"). This issue's filter is scoped to the single day currently shown on the Timeline.

## Key design decisions (confirmed with user during brainstorming)

1. **Existing `event_stat_id=4` "confirmed" status is left as-is.** The new tracking is a fully independent field, not a replacement. Both concepts can coexist in the UI without conflict, at the cost of two visually distinct "confirmed"-adjacent concepts — accepted as the safer choice (avoids touching the reminder-eligibility filter or any existing data).
2. **Confirming a virtual (unmaterialized) recurring occurrence materializes it**, using the exact same mechanism single-occurrence edits already use (`event_info.cpp`'s `RecurringEditScope::SingleOccurrence` path: reset `id = -1`, call `QTimelineWidget::addEvent`). No `EventSeriesException` is added — this isn't a reschedule or deletion, only a confirmation flag change; the occurrence's time is otherwise untouched. This reuses `Database::get_materialized_occurrence_starts_for_series` (from #38) to ensure the now-materialized occurrence doesn't get double-rendered as a virtual occurrence too.

## Architecture

### Data model

- New nullable `confirmed_at TIMESTAMP` column on `Event` (additive `ALTER TABLE ... ADD COLUMN IF NOT EXISTS`, matching every prior additive schema change in this codebase — no `schema_version` bump). NOT added to `EventSeries`: confirmation is inherently per-occurrence, not a series-level default.
- `DuckEvent` struct gains `std::optional<std::int64_t> confirmed_at`, read from the last column of the chunk (backward-compatible `if (chunk.ColumnCount() > N)` guard, matching `series_id`/`original_occurrence_start`/etc.).
- `kInsertEventQuery`/`kUpdateEventQuery` gain a `confirmed_at` column and one additional bound parameter. No new `Database` method is needed — `confirmed_at` is just another field on the `DuckEvent` passed into the existing `add_event`/`update_event`, exactly like `cost` or `cancellation_reason` already are.

### `QTimelineModel`

New method `setEventConfirmed(int64_t id, bool confirmed)`:
- Looks up the event by `id` in `mEvents` (same linear-scan pattern `removeEvent`/`updateEvent` already use).
- Builds a copy with `confirmed_at` set to "now" (confirmed) or `std::nullopt` (unconfirmed).
- If the found event `is_virtual_occurrence`: reset `id = -1`, call `mDb->add_event(...)` (materializing it, mirroring `event_info.cpp`'s existing single-occurrence-edit materialization), then replace the virtual entry in `mEvents` with the newly materialized one and emit the appropriate model signals.
- Otherwise: call `mDb->update_event(...)` on the existing row, update `mEvents[i]` in place, emit `dataChanged`.

New method `setUnconfirmedOnlyFilter(bool enabled)`: stores the flag and, when `loadEventsForDay()` next runs (called immediately if a day is already loaded), excludes events where `confirmed_at.has_value()` from the populated `mEvents`. This only affects standalone/materialized events with a real confirmation state — virtual occurrences are always "unconfirmed" by definition and are never excluded by this filter.

### UI — quick actions (`QEventItem` → `QEventView` → `QTimelineWidget` → `event_info.cpp`)

- `QEventItem` gains a `confirmed_at`-derived boolean member (mirroring how `mSeriesId`/`mIsVirtualOccurrence` are already threaded through the constructor/`updateFromEvent`/`toEvent`), and its `contextMenuEvent` gains two new actions, following the exact placement/pattern of the existing "Copy meeting invite" block:
  - "Mark confirmed" / "Mark unconfirmed" (label toggles based on current state) — emits a new signal `confirmToggleRequested()`.
  - "Copy confirmation request" — self-contained clipboard copy (no round-trip through the model), mirroring `pcm::meeting::copyMeetingInvite`.
- `confirmToggleRequested()` propagates up exactly like `deleteRequested()` already does: `QEventView::onEventConfirmToggleRequested()` → `eventConfirmToggleRequested(int64_t eventId)` → `QTimelineWidget` re-emits the same signal and exposes `confirmToggleEvent(int64_t id) const` (mirroring `removeEvent`) → `event_info.cpp` connects the signal to that method, mirroring the existing delete-wiring exactly.

### UI — visual indicator

`QEventItem::paint()` draws a small filled circle badge (green, ~8px) in a corner of the card when the event is confirmed — a `QPainter::drawEllipse` addition next to the existing icon-drawing code. No new SVG asset needed.

### UI — filter

`QTimelineWidget` gains a small checkbox ("Show only unconfirmed") placed above `mEventView` in its existing `QVBoxLayout`. Toggling it calls `QTimelineModel::setUnconfirmedOnlyFilter` and triggers a reload of the currently-shown day.

### Settings — confirmation request template

New `app_settings::confirmationRequestTemplate()`/`setConfirmationRequestTemplate()`, following the exact `meetingInviteTemplate` pattern (same placeholder substitution style: `{client_name}`, `{date}`, `{time}`). New function `pcm::confirmation::buildConfirmationRequestText(...)` / `copyConfirmationRequest(...)` in a new `src/widgets/confirmation_utils.h`/`.cpp`, mirroring `src/widgets/meeting_utils.h`/`.cpp` exactly. `SettingsDialog` gains a `QTextEdit` for the template, following the existing `mMeetingInviteTemplateEdit` row pattern.

## Error Handling

- Materializing a virtual occurrence's confirmation follows `addEvent`'s existing failure handling (returns `0`/`false` on conflict or DB error; the caller already logs and no-ops on failure, matching `event_info.cpp`'s existing materialization-failure path).
- Copying the confirmation request text has no failure mode beyond an empty clipboard (matches `copyMeetingInvite`'s behavior — no error dialog).

## Testing

- `Database::add_event`/`update_event` round-trip test for `confirmed_at` (`test/database_tests.cpp`), following the existing style of field round-trip tests in that file.
- Restore round-trip test verifying `confirmed_at` survives backup/restore (`test/backup_tests.cpp`), per `AGENTS.md`'s "schema changes require a migration and a restore/round-trip test" rule — mirrors the existing `RestoresSeriesOccurrenceReminderState`-style test.
- No dedicated unit test for `pcm::confirmation::buildConfirmationRequestText()`/`copyConfirmationRequest()`: confirmed that `src/widgets/meeting_utils.cpp` (the pattern this mirrors exactly) has zero existing test coverage — no test target links `PsyClientManager_widgets`. Following that established precedent, `confirmation_utils.cpp` is verified via build success only, consistent with how `meeting_utils.cpp` is handled today.
