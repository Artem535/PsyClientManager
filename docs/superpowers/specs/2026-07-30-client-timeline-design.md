# Unified Client Timeline — Design

Issue: #30

## Goal

Give each client a single chronological history mixing sessions (events)
and notes, instead of two disconnected screens. Concretely: evolve
`ClientNotesPage` (built in #48 as a date-grouped, chat-like note feed)
into the full timeline by interleaving compact session entries into the
same feed, add a "last/next appointment" summary to both that screen's
header and the client card, and add a type filter (Все / Встречи /
Заметки).

## Current state

- `ClientNotesPage` already has: date-divided feed, breadcrumb header with
  a client-card link, compact click-to-expand attachments, a fast composer.
  It fetches notes via `Database::get_client_notes(client_id)`.
- There is no `Database` method that fetches events for a client. Events
  reach a client two different ways:
  - **Materialized/standalone events** — linked via the `EventClient`
    many-to-many join table (`Database::add_event_client`).
  - **Recurring series** — `EventSeries` has a *direct* `client_id` column,
    but individual occurrences are virtual (`is_virtual_occurrence = true`,
    `id = -1`) until something materializes them (confirm, edit, etc., per
    the pattern from #21/#38). A client's full recurring history is **not**
    a simple join — it requires expanding `EventSeries.recurrence_rule`
    into occurrences via `pcm::recurrence::occurrences()` /
    `buildVirtualOccurrence()`, the same helpers `QTimelineModel` already
    uses for the day view, skipping dates in
    `EventSeriesException`/already-materialized starts (via
    `get_event_series_exceptions_for_range` /
    `get_materialized_occurrence_starts_for_series`, both already exist).
- `QClientInfoCardPage` embeds `ClientChartsWidget` by replacing a
  `graphicsPlaceholder` widget from the `.ui` file
  (`verticalLayout_3->replaceWidget(...)`) — the same pattern will be used
  to add the appointment-summary widget above the charts, added
  programmatically rather than editing the `.ui` XML.

## Scope decisions (confirmed)

- The aggregated feed lives on the existing Notes screen/nav slot — no new
  page, no new nav entry.
- Virtual recurring occurrences appear in the feed, but only within a
  bounded window (~3 months back / ~3 months forward) around "now" —
  materialized/standalone events are **not** windowed (they're just plain
  indexed rows, nothing expensive about fetching all of them).
- "Last/next appointment" is shown in both the timeline header and on the
  client card.
- A type filter (Все / Встречи / Заметки) ships in this pass — unlike the
  attachments-only filter removed from #48, this splits genuinely
  different entry kinds, not a sub-filter of one kind.
- **Out of scope this pass:** clicking a session entry to jump into the
  Calendar/editor (separate cross-page navigation work); synthetic
  "payment status changed" / "rescheduled" system-log lines (no change
  history is stored anywhere — would need new schema, not just new
  queries — tracked as a future idea, not built speculatively now).

## Backend

### `Database::get_events_for_client(int64_t client_id)`

```cpp
std::vector<DuckEvent> get_events_for_client(int64_t client_id);
```

New query, mirroring the existing `EventClient` join used by
`kSelectClientMonthlyStatsQuery`:

```sql
SELECT e.* FROM Event e
JOIN EventClient ec ON ec.event_id = e.id
WHERE ec.client_id = $1
ORDER BY e.start_date ASC
```

Unbounded — every materialized event ever linked to this client.

### `Database::get_event_series_for_client_and_range(int64_t client_id, int64_t range_start_ms, int64_t range_end_ms)`

```cpp
std::vector<DuckEventSeries> get_event_series_for_client_and_range(
    int64_t client_id, int64_t range_start_ms, int64_t range_end_ms);
```

Mirrors `kSelectEventSeriesForRangeQuery`, adding the client filter:

```sql
SELECT * FROM EventSeries
WHERE client_id = $1
  AND active = TRUE
  AND start_date <= $2
  AND (recurrence_until IS NULL OR recurrence_until >= $3)
```

### `pcm::recurrence::eventsForClient`

New free function in `recurrence_utils.h/.cpp` (same module as the
existing `occurrences`/`buildVirtualOccurrence`/`resolveSeriesClientName`,
since it composes the same recurrence-expansion building blocks):

```cpp
QVector<DuckEvent> eventsForClient(pcm::database::Database &db,
                                   int64_t clientId,
                                   const QDateTime &virtualWindowStart,
                                   const QDateTime &virtualWindowEnd);
```

Implementation: fetch `get_events_for_client(clientId)` (materialized),
then for each series from `get_event_series_for_client_and_range(clientId,
windowStartMs, windowEndMs)`, expand occurrences within the window exactly
like `QTimelineModel::loadEventsForDay` does — skipping
`EventSeriesException` dates and already-materialized starts — append the
virtual ones, sort the combined vector by `start_date`.

### `pcm::recurrence::lastAndNextAppointment`

Pure function, no DB access — takes the already-fetched event vector:

```cpp
struct LastNextAppointment {
  std::optional<DuckEvent> last;
  std::optional<DuckEvent> next;
};

LastNextAppointment lastAndNextAppointment(const QVector<DuckEvent> &events,
                                           qint64 nowMs);
```

Considers only events with `event_stat_id` in {1 (Scheduled), 2
(Completed), 4 (Confirmed)} — Canceled/No-show/Rescheduled don't count as
"the appointment". `last` = closest with `start_date < nowMs`, `next` =
closest with `start_date >= nowMs`.

## UI

### `ClientNotesPage` feed

- `reloadNotes()` now also calls `pcm::recurrence::eventsForClient(...)`
  for the current client (window: today ± 3 months) and merges the result
  with notes into one chronologically sorted sequence before doing the
  existing date-divider pass.
- New `addSessionEntry(const DuckEvent &event)` renders a compact two-line
  card, visually distinct from note bubbles (different background/border,
  no markdown body): first line time + title, second line status badge +
  payment + cost, or the cancellation reason/initiator if canceled.
- New type filter using `oclero::qlementine::SegmentedControl` (not
  `QComboBox` — see the crash fixed in #48: constructing/populating a
  plain `QComboBox` before the main window's first `show()` triggers a
  reentrancy bug in `oclero::qlementine`'s combobox popup construction;
  `SegmentedControl` is already used safely in this exact pre-show timing
  elsewhere, e.g. `qevent_details_widget.cpp`, `analytics_page.cpp`).
- Header gains a last/next-appointment line under the breadcrumb, e.g.
  `Последняя: 12 июля · Следующая: 30 июля, 15:00`.

### `QClientInfoCardPage`

- A small summary widget (last/next appointment, same computation) is
  created programmatically in the constructor and inserted into
  `verticalLayout_3` above `mChartsWidget`, refreshed in `setClientInfo`
  alongside `refreshCharts()`.

## Testing

- `Database::get_events_for_client` — DB round-trip test: add a client, an
  event materialized and linked via `add_event_client`, verify it comes
  back; verify an event linked to a *different* client is excluded.
- `Database::get_event_series_for_client_and_range` — DB test: add a
  series with `client_id` set, verify it's returned within range and
  excluded outside range or for a different client.
- `pcm::recurrence::eventsForClient` — DB-backed test: client with one
  materialized event + one active weekly series; verify the result
  contains the materialized event plus virtual occurrences within the
  window, and that a materialized occurrence of that same series isn't
  duplicated as a virtual one (via `get_materialized_occurrence_starts_for_series`).
- `pcm::recurrence::lastAndNextAppointment` — pure unit tests (no DB): past
  event only → `next` empty; future event only → `last` empty; mixed
  events including a canceled one closer to "now" than a real one →
  canceled is skipped.
- No dedicated UI test for the `ClientNotesPage`/`QClientInfoCardPage`
  rendering changes — consistent with their existing lack of test-target
  linkage; verified by clean build (as done throughout #21/#25/#48).

## Out of scope (deferred)

- Clicking a session entry to open the Calendar/event editor.
- Synthetic system-log lines for status/payment changes (needs a change-
  history table, not present).
- In-place note editing (#28).
- Cross-client search.
