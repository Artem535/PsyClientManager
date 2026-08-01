# Event Change Log (Client Timeline System Lines) — Design

**Status:** Approved
**Related issue:** #57 ("Finish the client journal"), system event log lines part (part 2 of 2 — part 1 shipped in PR #58)

## Goal

Show lightweight, read-only history lines in a client's Notes feed whenever a
session's status, payment status, or scheduled time changes — e.g.:

> Встреча перенесена с 3 августа на 5 августа
> Статус оплаты изменён: ожидается → оплачено

so the feed reflects the history of a client's sessions, not just their
current snapshot.

## Scope

- Log three kinds of change to a single (non-series) event: event status
  change, payment status change, and reschedule (start/end time change).
- Render each as a small, centered, neutral line in the client's Notes feed,
  interleaved chronologically with notes and session cards.
- Clicking a line navigates to that event, matching the existing behavior of
  session cards and note session-badges.
- Include the change in backup/restore round-trips.

## Out of scope

- Logging changes made through `update_event_series` (whole-series edits).
  A single series edit can touch many future occurrences at once, and there's
  no single event to anchor one log line to. Deferred to a future issue if
  needed.
- Any kind of undo, audit trail beyond display, or edit/delete of a log line.
- Changes to newly created events (`add_event`) — there is no prior state to
  diff against, so no log line is produced on creation.

## Where the log gets written

`Database::update_event(const DuckEvent &event, bool allowOverlap)` is the
single choke point every event edit already goes through (dialog save today;
any future edit path — e.g. drag-to-reschedule on the timeline — for free).
Before overwriting the row, `update_event` now:

1. Reads the existing row for `event.id`.
2. Diffs it against the incoming `event`:
   - `event_stat_id` differs → insert a "status" change row.
   - `payment_stat_id` differs → insert a "payment" change row.
   - `start_date` or `end_date` differs → insert a "reschedule" change row.
3. Performs the existing update.

All of this happens on the same `duckdb::Connection` as the update. A single
save that changes multiple aspects (e.g. marks a session Completed *and*
shifts its time) produces multiple rows — one per changed fact — rather than
one row trying to encode several facts.

`Database::remove_event` gains a companion delete of `EventChangeLog` rows for
that `event_id`, mirroring how it already deletes `EventClient` rows before
deleting the event (DuckDB here does not enforce `ON DELETE CASCADE`).

## Schema

```sql
CREATE TABLE IF NOT EXISTS EventChangeLog (
    id INTEGER PRIMARY KEY,
    event_id INTEGER NOT NULL REFERENCES Event(id),
    change_kind INTEGER NOT NULL, -- 1=status, 2=payment, 3=reschedule
    old_event_stat_id INTEGER,
    new_event_stat_id INTEGER,
    old_payment_stat_id INTEGER,
    new_payment_stat_id INTEGER,
    old_start_date TIMESTAMP,
    new_start_date TIMESTAMP,
    cancellation_reason TEXT,
    occurred_at TIMESTAMP NOT NULL
);
```

Only the columns relevant to a row's `change_kind` are populated; the rest
stay `NULL`. No separate lookup table, no generic key/value encoding — one
row shape covers all three change kinds because each kind only ever touches a
disjoint subset of columns.

`cancellation_reason` is populated only on a "status" row where
`new_event_stat_id` is Canceled (3) and the event has a non-empty
`cancellation_reason`, so the rendered line can fold the reason in:
"Status changed: Scheduled → Canceled (Client canceled: ran out of time)"
instead of needing a second line.

Status/payment IDs are stored as the same raw numeric IDs already used by
`Event.event_stat_id`/`payment_stat_id` (see `kEventStatus`/`kPaymentStatus`
reference tables). Display text (e.g. `tr("Scheduled")`, `tr("Paid")`) is
produced only at render time in C++, never persisted as localized text —
consistent with the app's existing localization approach (and the concern
behind issue #47, unifying RU/EN strings).

This is an idempotent addition to the existing `kCreateTables` block in
`src/database/constants.hpp` — no separate migration file exists in this
codebase; schema changes are additive `CREATE TABLE IF NOT EXISTS` /
`ALTER TABLE ... ADD COLUMN IF NOT EXISTS` statements applied at startup via
`Database::init_tables()` / `Database::apply_schema_migrations()`.

### Backup/restore

`EXPORT DATABASE` / `IMPORT DATABASE` (used by `BackupService` /
`RestoreService`) dumps the whole schema, so `EventChangeLog` round-trips
automatically with no extra wiring. Per `AGENTS.md` ("Database schema changes
require a migration and a restore/round-trip test"), a new
`RestoreServiceTest` case will assert this explicitly.

## Retrieval

New `Database::get_event_change_log_for_client(int64_t clientId)` joins
`EventChangeLog` → `Event` → `EventClient`, the same join shape
`get_events_for_client` already uses, returning change-log rows for events
linked to that client. Returns a new `DuckEventChangeLog` value type (mirrors
the existing `DuckEvent`/`DuckClientNote` pattern in `schema.hpp`).

## Feed integration

`ClientNotesPage::reloadNotes` currently merges
`std::variant<DuckClientNote, DuckEvent>` into one chronologically sorted,
date-divided feed, filtered by `FeedFilter::{All, Sessions, Notes}`. This
grows a third alternative:

```cpp
using FeedItem = std::variant<DuckClientNote, DuckEvent, DuckEventChangeLog>;
```

- **Sort key:** `occurred_at` — when the change was made, not the session's
  (now possibly-changed) `start_date`. A reschedule made today shows up in
  today's section of the feed, not glued to the session card's new date. This
  matches the feature's framing as a *history of actions*, not a snapshot.
- **Filter visibility:** shown under `All` and `Sessions` (it's about a
  session), hidden under `Notes`.
- **Rendering:** new `ClientNotesPage::addChangeLogEntry(const
  DuckEventChangeLog&)` adds a small, centered, neutral-styled line — visually
  similar to the existing date dividers, not a chat bubble. Example lines:
  - "Status changed: Scheduled → Completed"
  - "Payment status changed: Pending → Paid"
  - "Rescheduled from Aug 3, 14:00 to Aug 5, 15:00"
  - "Status changed: Scheduled → Canceled (Client canceled: ran out of time)"
- **Interaction:** clicking a line emits the same
  `ClientNotesPage::openEventRequested(eventId, dayMs)` signal that session
  cards and note session-badges already emit, routed through the existing
  `MainWindow` → `QEventInfoPage::openEventOnDay` path — no new navigation
  plumbing needed.

## Testing plan

- `DatabaseTest`: `update_event` produces the right `EventChangeLog` row(s)
  for a status-only change, a payment-only change, a reschedule, and a
  combined change (asserting one row per changed fact, not one merged row).
  A no-op update (nothing actually changed) produces zero rows. `add_event`
  produces zero rows. `remove_event` deletes any `EventChangeLog` rows for
  that event.
- `RestoreServiceTest`: a backup containing `EventChangeLog` rows restores
  them unchanged (round-trip coverage per `AGENTS.md`).
- `ClientNotesPage`-adjacent logic (wherever `FeedItem` sorting/filtering is
  unit-testable, or via the recurrence/feed-building helpers if extracted):
  change-log entries interleave correctly by `occurred_at`, respect the
  `Sessions`/`Notes`/`All` filter, and render the four example line shapes
  above with correct old→new values and the folded-in cancellation reason.

## Version/changelog

Per `AGENTS.md`, this MR bumps `CMakeLists.txt` / `application.cpp` and adds a
`CHANGELOG.md` entry, and closes #57 (`Closes #57`) since this is the
remaining half of that issue.
