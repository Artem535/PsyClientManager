# Day Summary Panel — Design

**Issue:** #44 — "[P0] Improve left-panel layout: calendar, quick slots, and day summary"

## Problem

The left column of the main Calendar page (`QEventInfoPage`, `ui/pages/eventinfo.ui`)
has a large dead zone: `calendarCard` → `calendarBottomSpacer` (an expanding
vertical spacer) → `QuickSlotsWidget` (inserted programmatically at
`verticalLayout` index 2) → a `QLabel` named `label` whose text
(`FREE_SLOTS_PLACEHOLDER`) is never set and which is immediately hidden in
`QEventInfoPage`'s constructor. The spacer swallows all leftover height,
so the column reads as two disconnected widgets with empty space between
them instead of one coherent planning tool.

## Goal

Close the gap with a compact day-summary block, and reorder the column so
it reads top-to-bottom as one flow:

```
Calendar
↓
Day summary (for the selected date)
↓
Quick Slots
```

## Scope

- New `DaySummaryWidget` + a pure `pcm::recurrence::computeDaySummary(...)`
  function.
- Reorder `eventinfo.ui`'s left column; remove the dead spacer and
  placeholder label.
- A lightweight "highlight on timeline" mechanism for the summary's
  mini appointment list, separate from the existing click-to-edit flow.
- Out of scope: any change to Quick Slots' own generation logic (tracked
  separately per #44, related to #26).

## Data & computation rules

`pcm::recurrence::computeDaySummary(events, busyIntervals, workDayStart,
workDayEnd, selectedDate, nowMs)` — a new pure function alongside
`lastAndNextAppointment` in `src/event_view/recurrence_utils.h/.cpp`
(same file, same `pcm::recurrence` namespace, no Qt-widget dependency —
directly unit-testable the same way).

- `events`: the day's events, as already loaded into `mTimelineWidget`
  (`QTimelineWidget::events()`, populated by
  `onSelectedDayChanged(date)`).
- `busyIntervals`: `QEventInfoPage::currentBusyIntervals()` — already
  includes every event (work and personal) with buffers applied, exactly
  what feeds `QuickSlotsWidget` today. Used only for the free-window
  gap search, so free time correctly accounts for personal events too.
- Counts, busy minutes, "next session", and the mini-list only consider
  Work events (`is_work_event`) with `event_stat_id` in
  `{1 scheduled, 2 completed, 4 confirmed}` — the exact same filter
  `lastAndNextAppointment` already uses (excludes canceled/no-show/
  rescheduled). Busy minutes sums `(end_date - start_date)` for those
  events, independent of the buffer-padded `busyIntervals`.
- "Next session" = the nearest qualifying event with `start_date >
  nowMs`. Meaningful only when `selectedDate` is today or later *and*
  `nowMs` is still before `workDayEnd` on that date; otherwise omitted.
- "Free window" = the first open gap of at least
  `pcm::app_settings::defaultSessionDurationMinutes()` between entries
  of `busyIntervals` — the same duration `QuickSlotsWidget` already uses
  to decide what counts as a usable slot — bounded by `[workDayStart,
  workDayEnd]` on `selectedDate`, starting the search from
  `max(workDayStart, nowMs)` when `selectedDate` is today. Omitted under
  the same past-day/past-work-hours condition as "next session".
- Mini-list: up to 3 nearest-upcoming qualifying events, chronological.
  For a past day, "upcoming" has no meaning, so the mini-list instead
  shows that day's qualifying events in chronological order (what
  happened), still capped at 3.
- Zero qualifying events for `selectedDate` → summary reports an empty
  state (`hasSessions = false`); the widget shows "Свободен весь день"
  and the date, no counts, no mini-list.

## Components

### `DaySummary` struct (new, `recurrence_utils.h`)

```cpp
struct DaySummary {
  QDate date;
  bool hasSessions = false;
  int sessionCount = 0;
  int clientCount = 0;
  qint64 busyMinutes = 0;
  std::optional<DuckEvent> nextSession;
  std::optional<QDateTime> freeWindowStart;
  std::optional<QDateTime> freeWindowEnd;
  QVector<DuckEvent> upcoming; // up to 3, see rules above
};

DaySummary computeDaySummary(const QVector<DuckEvent> &events,
                             const QVector<QPair<QDateTime, QDateTime>> &busyIntervals,
                             QTime workDayStart, QTime workDayEnd,
                             const QDate &selectedDate, qint64 nowMs);
```

`clientCount` is the number of distinct `client_name` values among the
day's qualifying events (falls back to counting distinct event ids if
`client_name` is empty for a given entry, so it never under-reports).

### `DaySummaryWidget` (new, `src/pages/event_info_page/`)

Presentation-only, follows the same shape as `AppointmentSummaryWidget`
(`src/pages/detail_client_info_page/appointment_summary_widget.h`):

```cpp
class DaySummaryWidget final : public QWidget {
  Q_OBJECT
public:
  explicit DaySummaryWidget(QWidget *parent = nullptr);
public slots:
  void setSummary(const pcm::recurrence::DaySummary &summary);
signals:
  void eventHighlightRequested(int64_t eventId);
};
```

Renders the date heading, "N сессий · M клиентов", busy time, next
session line, free-window line, and up to 3 clickable mini-list rows
(time + client name + status label, matching the existing
`addSessionEntry` style already used in `ClientNotesPage` for
consistency). A row click emits `eventHighlightRequested`. No
qlementine custom-painted control is hosted here, so a plain
`setStyleSheet()` card surface is fine — the `SegmentedControl`/
`QStyleSheetStyle` gotcha documented in `surface_paint_filter.h` doesn't
apply.

### Timeline highlight plumbing

- `QEventItem::setHighlighted(bool)` (new, `src/event_view/event_item.h/.cpp`)
  — a flag checked in `paint()` to draw an accent border/glow, mirroring
  how the class already tracks other visual state.
- `QEventView::highlightEvent(int64_t eventId)` (new,
  `src/event_view/event_view.h/.cpp`) — clears the flag on any
  previously-highlighted item via `mSceneItems`, sets it on the target
  (no-op if the id isn't in the currently-loaded day), and calls
  `centerOn(item)` so it scrolls into view.
- `QTimelineWidget::highlightEvent(int64_t eventId)` (new, forwards to
  `mEventView`).
- `QEventInfoPage` connects `mDaySummaryWidget::eventHighlightRequested`
  to `mTimelineWidget->highlightEvent`. This is independent of the
  existing `QTimelineWidget::eventSelected → editEventWithDialog` path,
  which still governs direct clicks on timeline cards.

### `eventinfo.ui` / `QEventInfoPage` wiring

- Remove `calendarBottomSpacer` and the `label`
  (`FREE_SLOTS_PLACEHOLDER`) items from `verticalLayout` in
  `ui/pages/eventinfo.ui` — both are dead weight today.
- In `QEventInfoPage`'s constructor: `mDaySummaryWidget = new
  DaySummaryWidget(this); mUi->verticalLayout->insertWidget(1,
  mDaySummaryWidget);` then insert `mQuickSlotsWidget` at index 2
  (was 2 already, now correctly follows the summary instead of the
  removed spacer/label).
- `onCalendarClicked(date)` and the initial `initDefaultStates()` path
  both already recompute `mSelectedDate` and call `refreshQuickSlots()`;
  add a parallel `refreshDaySummary()` called from the same two spots,
  which builds the `DaySummary` via `computeDaySummary(mTimelineWidget->events(),
  currentBusyIntervals(), pcm::app_settings::workDayStart(),
  pcm::app_settings::workDayEnd(), mSelectedDate, QDateTime::currentDateTimeUtc().toMSecsSinceEpoch())`
  and calls `mDaySummaryWidget->setSummary(...)`.
- Also call `refreshDaySummary()` wherever `refreshQuickSlots()` is
  already called after data changes (`onEventSaved`,
  `onTimelineEventDeleteRequested`'s three branches, `refreshAppearance()`)
  so the summary stays in sync with the timeline.

## Testing

- `computeDaySummary` unit tests in `test/recurrence_utils_tests.cpp`
  (same file/target as `lastAndNextAppointment`'s tests):
  - Empty day → `hasSessions = false`, no mini-list.
  - Fully booked work day → no free window.
  - Free-window search respects buffered `busyIntervals`, not just raw
    event times.
  - Canceled/no-show/rescheduled events excluded from count, busy
    minutes, and mini-list, but still present in `busyIntervals` (so
    they still block free-window search — a canceled personal event
    doesn't free up time it never occupied on the calendar).
  - Past-day input omits `nextSession` and free window, mini-list shows
    that day's qualifying events instead of "upcoming".
  - Today, but `nowMs` past `workDayEnd` → same omission as a past day.
  - Mini-list capped at 3, chronological.
  - `clientCount` counts distinct clients, not distinct sessions.
- `DaySummaryWidget` and the timeline-highlight change are smoke-tested
  manually (matches this codebase's convention — Qt widgets aren't
  unit-tested here, only pure logic is).

## Out of scope / explicitly deferred

- Any visual/behavioral change to Quick Slots itself (issue #26).
- A "highlight" visual language beyond a simple accent border — no new
  animation system.
