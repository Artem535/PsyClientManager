# Day Summary Panel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the dead zone in the Calendar page's left column (issue #44) by inserting a compact day-summary panel between the calendar and Quick Slots, and give its mini appointment list a lightweight "highlight on timeline" interaction.

**Architecture:** A new pure function `pcm::recurrence::computeDaySummary(...)` (alongside the existing `lastAndNextAppointment`) turns the selected day's events + busy intervals into a `DaySummary` value struct. A new presentation-only `DaySummaryWidget` renders that struct and emits `eventHighlightRequested(eventId)` on mini-list clicks. `QEventInfoPage` wires calendar-day-change → `computeDaySummary` → `DaySummaryWidget::setSummary`, and wires `eventHighlightRequested` → a new `QTimelineWidget::highlightEvent(eventId)` that flags the matching `QEventItem` and scrolls it into view — separate from the existing click-to-edit path.

**Tech Stack:** C++20, Qt6 Widgets, DuckDB (via `pcm::database::Database`), GoogleTest/ctest.

## Global Constraints

- Every DB/schema change needs a migration + round-trip test — **N/A for this plan**, no schema changes.
- Every merge needs an issue, branch, version bump, and CHANGELOG entry. Issue: #44. Branch: `feat/44-day-summary-panel` (already created, spec committed on it as `ae7eec8`).
- Bump `PROJECT_VERSION` in `CMakeLists.txt` and `app.setApplicationVersion(...)` in `src/app/application.cpp` together; add a `CHANGELOG.md` entry. Next version: `0.1.20`.
- Counts/busy-time/mini-list logic must reuse the exact "Scheduled(1)/Completed(2)/Confirmed(4)" qualifying-status filter already used by `lastAndNextAppointment` (`src/event_view/recurrence_utils.cpp:218`) — don't invent a new status set.
- The free-window search must use `pcm::app_settings::defaultSessionDurationMinutes()` as its minimum gap length — the same value `QuickSlotsWidget` already uses to decide what counts as a usable slot.
- `DaySummaryWidget` hosts no qlementine custom-painted control (no `SegmentedControl`), so a plain `setStyleSheet()` card surface is fine — the `SurfacePaintFilter` workaround (`src/widgets/surface_paint_filter.h`) does not apply here and must not be added speculatively.

---

### Task 1: `computeDaySummary` pure function

**Files:**
- Modify: `src/event_view/recurrence_utils.h`
- Modify: `src/event_view/recurrence_utils.cpp`
- Test: `test/recurrence_utils_tests.cpp`

**Interfaces:**
- Consumes: `DuckEvent` (`src/database/schema.hpp`) fields `is_work_event`, `event_stat_id`, `start_date`, `end_date`, `client_name`.
- Produces:
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
    QVector<DuckEvent> upcoming; // up to 3, chronological
  };

  DaySummary computeDaySummary(const QVector<DuckEvent> &events,
                               const QVector<QPair<QDateTime, QDateTime>> &busyIntervals,
                               QTime workDayStart, QTime workDayEnd,
                               const QDate &selectedDate, qint64 nowMs,
                               int minFreeWindowMinutes);
  ```
  Consumed by Task 3 (`DaySummaryWidget::setSummary`) and Task 4 (`QEventInfoPage::refreshDaySummary`).

- [ ] **Step 1: Write the failing tests**

Add to `test/recurrence_utils_tests.cpp`, right after the existing `eventAt` helper (around line 20):

```cpp
DuckEvent workSessionAt(const int64_t id, const int64_t startMs, const int64_t endMs,
                       const QString &clientName = QStringLiteral("Client"),
                       const int64_t eventStatId = 1) {
  DuckEvent event;
  event.id = id;
  event.is_work_event = true;
  event.start_date = startMs;
  event.end_date = endMs;
  event.event_stat_id = eventStatId;
  if (!clientName.isEmpty()) {
    event.client_name = clientName.toStdString();
  }
  return event;
}
```

Then add these tests at the end of the file (before the final nothing — i.e. appended after the last existing `TEST(...)` block):

```cpp
TEST(RecurrenceUtilsTest, ComputeDaySummaryEmptyDayHasNoSessions) {
  const auto date = QDate::currentDate();
  const auto noon =
      QDateTime(date, QTime(12, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();

  const auto summary = pcm::recurrence::computeDaySummary(
      {}, {}, QTime(9, 0), QTime(18, 0), date, noon, 50);

  EXPECT_FALSE(summary.hasSessions);
  EXPECT_EQ(summary.sessionCount, 0);
  EXPECT_EQ(summary.clientCount, 0);
  EXPECT_EQ(summary.busyMinutes, 0);
  EXPECT_FALSE(summary.nextSession.has_value());
  EXPECT_FALSE(summary.freeWindowStart.has_value());
  EXPECT_TRUE(summary.upcoming.isEmpty());
}

TEST(RecurrenceUtilsTest, ComputeDaySummaryFullyBookedDayHasNoFreeWindow) {
  const auto date = QDate::currentDate();
  const auto startMs =
      QDateTime(date, QTime(9, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
  const auto endMs =
      QDateTime(date, QTime(18, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
  const auto beforeStartMs =
      QDateTime(date, QTime(8, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();

  const QVector<DuckEvent> events{workSessionAt(1, startMs, endMs)};
  const QVector<QPair<QDateTime, QDateTime>> busy{
      {QDateTime::fromMSecsSinceEpoch(startMs, QTimeZone::systemTimeZone()),
       QDateTime::fromMSecsSinceEpoch(endMs, QTimeZone::systemTimeZone())}};

  const auto summary = pcm::recurrence::computeDaySummary(
      events, busy, QTime(9, 0), QTime(18, 0), date, beforeStartMs, 50);

  EXPECT_TRUE(summary.hasSessions);
  EXPECT_FALSE(summary.freeWindowStart.has_value());
  EXPECT_FALSE(summary.freeWindowEnd.has_value());
}

TEST(RecurrenceUtilsTest, ComputeDaySummaryFreeWindowRespectsBusyIntervalsNotJustEvents) {
  const auto date = QDate::currentDate();
  const auto eventStartMs =
      QDateTime(date, QTime(10, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
  const auto eventEndMs =
      QDateTime(date, QTime(11, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
  const auto beforeMs =
      QDateTime(date, QTime(8, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();

  const QVector<DuckEvent> events{workSessionAt(1, eventStartMs, eventEndMs)};
  // Busy interval starts at the work day's start (no gap ahead of it) and
  // extends well past the event's own 10:00-11:00 bounds (buffer / personal
  // event) - proving the search follows busyIntervals, not the event's own
  // start/end.
  const QVector<QPair<QDateTime, QDateTime>> busy{
      {QDateTime(date, QTime(9, 0, 0), QTimeZone::systemTimeZone()),
       QDateTime(date, QTime(14, 0, 0), QTimeZone::systemTimeZone())}};

  const auto summary = pcm::recurrence::computeDaySummary(
      events, busy, QTime(9, 0), QTime(18, 0), date, beforeMs, 50);

  ASSERT_TRUE(summary.freeWindowStart.has_value());
  EXPECT_GE(summary.freeWindowStart->time(), QTime(14, 0, 0));
}

TEST(RecurrenceUtilsTest, ComputeDaySummaryExcludesCanceledFromCountsButKeepsItBlocking) {
  const auto date = QDate::currentDate();
  // A qualifying (scheduled) event early in the day, purely so hasSessions
  // is true - its own slot is already in the past by `now` below, so it
  // can't itself supply the free window this test checks for.
  const auto qualifyingStartMs =
      QDateTime(date, QTime(9, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
  const auto qualifyingEndMs =
      QDateTime(date, QTime(9, 15, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
  // A canceled event later in the day - excluded from counts, but its
  // interval is still passed via busyIntervals (exactly what
  // QEventInfoPage::currentBusyIntervals() does today - it doesn't filter
  // by status).
  const auto canceledStartMs =
      QDateTime(date, QTime(10, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
  const auto canceledEndMs =
      QDateTime(date, QTime(11, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
  // "Now" sits inside the canceled event's slot, so there's no earlier gap
  // for the search to find - the only candidate gap is whatever remains
  // after the canceled interval ends.
  const auto nowMs =
      QDateTime(date, QTime(10, 30, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();

  const QVector<DuckEvent> events{
      workSessionAt(1, qualifyingStartMs, qualifyingEndMs),
      workSessionAt(2, canceledStartMs, canceledEndMs, QStringLiteral("Anna"),
                    /*eventStatId=*/3)};
  const QVector<QPair<QDateTime, QDateTime>> busy{
      {QDateTime::fromMSecsSinceEpoch(qualifyingStartMs, QTimeZone::systemTimeZone()),
       QDateTime::fromMSecsSinceEpoch(qualifyingEndMs, QTimeZone::systemTimeZone())},
      {QDateTime::fromMSecsSinceEpoch(canceledStartMs, QTimeZone::systemTimeZone()),
       QDateTime::fromMSecsSinceEpoch(canceledEndMs, QTimeZone::systemTimeZone())}};

  const auto summary = pcm::recurrence::computeDaySummary(
      events, busy, QTime(9, 0), QTime(18, 0), date, nowMs, 50);

  EXPECT_TRUE(summary.hasSessions);
  EXPECT_EQ(summary.sessionCount, 1); // canceled event not counted
  EXPECT_EQ(summary.busyMinutes, 15); // only the qualifying event's span
  // The canceled event's interval still blocks the free-window search past
  // its own end time, even though it doesn't count as a session.
  ASSERT_TRUE(summary.freeWindowStart.has_value());
  EXPECT_GE(summary.freeWindowStart->time(), QTime(11, 0, 0));
}

TEST(RecurrenceUtilsTest, ComputeDaySummaryPastDayOmitsNextSessionAndFreeWindow) {
  const auto date = QDate::currentDate().addDays(-1);
  const auto startMs =
      QDateTime(date, QTime(10, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
  const auto endMs =
      QDateTime(date, QTime(11, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
  const auto nowMs = QDateTime::currentDateTime().toUTC().toMSecsSinceEpoch();

  const QVector<DuckEvent> events{workSessionAt(1, startMs, endMs)};

  const auto summary = pcm::recurrence::computeDaySummary(
      events, {}, QTime(9, 0), QTime(18, 0), date, nowMs, 50);

  EXPECT_TRUE(summary.hasSessions);
  EXPECT_FALSE(summary.nextSession.has_value());
  EXPECT_FALSE(summary.freeWindowStart.has_value());
  EXPECT_EQ(summary.upcoming.size(), 1);
}

TEST(RecurrenceUtilsTest, ComputeDaySummaryTodayAfterWorkHoursOmitsNextAndFreeWindow) {
  const auto date = QDate::currentDate();
  const auto startMs =
      QDateTime(date, QTime(10, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
  const auto endMs =
      QDateTime(date, QTime(11, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
  const auto lateNowMs =
      QDateTime(date, QTime(23, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();

  const QVector<DuckEvent> events{workSessionAt(1, startMs, endMs)};

  const auto summary = pcm::recurrence::computeDaySummary(
      events, {}, QTime(9, 0), QTime(18, 0), date, lateNowMs, 50);

  EXPECT_FALSE(summary.nextSession.has_value());
  EXPECT_FALSE(summary.freeWindowStart.has_value());
  EXPECT_EQ(summary.upcoming.size(), 1);
}

TEST(RecurrenceUtilsTest, ComputeDaySummaryUpcomingCappedAtThreeChronological) {
  const auto date = QDate::currentDate();
  const auto beforeMs =
      QDateTime(date, QTime(8, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();

  QVector<DuckEvent> events;
  for (int hour = 9; hour < 14; ++hour) {
    const auto startMs =
        QDateTime(date, QTime(hour, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
    const auto endMs =
        QDateTime(date, QTime(hour, 30, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
    events.append(workSessionAt(hour, startMs, endMs));
  }

  const auto summary = pcm::recurrence::computeDaySummary(
      events, {}, QTime(9, 0), QTime(18, 0), date, beforeMs, 50);

  ASSERT_EQ(summary.upcoming.size(), 3);
  EXPECT_EQ(summary.upcoming[0].id, 9);
  EXPECT_EQ(summary.upcoming[1].id, 10);
  EXPECT_EQ(summary.upcoming[2].id, 11);
}

TEST(RecurrenceUtilsTest, ComputeDaySummaryClientCountCountsDistinctClientsNotSessions) {
  const auto date = QDate::currentDate();
  const auto beforeMs =
      QDateTime(date, QTime(8, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
  const auto mkTimes = [&](const int hour) {
    return std::make_pair(
        QDateTime(date, QTime(hour, 0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch(),
        QDateTime(date, QTime(hour, 30, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch());
  };
  const auto [start1, end1] = mkTimes(9);
  const auto [start2, end2] = mkTimes(10);
  const auto [start3, end3] = mkTimes(11);

  const QVector<DuckEvent> events{
      workSessionAt(1, start1, end1, QStringLiteral("Anna")),
      workSessionAt(2, start2, end2, QStringLiteral("Anna")),
      workSessionAt(3, start3, end3, QStringLiteral("")),
  };

  const auto summary = pcm::recurrence::computeDaySummary(
      events, {}, QTime(9, 0), QTime(18, 0), date, beforeMs, 50);

  EXPECT_EQ(summary.sessionCount, 3);
  EXPECT_EQ(summary.clientCount, 2); // "Anna" once + 1 unnamed
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build-release --target PsyClientManager_recurrence_tests -j"$(nproc)"`
Expected: FAIL to compile — `computeDaySummary` and `DaySummary` are not declared yet.

- [ ] **Step 3: Add the declaration**

In `src/event_view/recurrence_utils.h`, add `#include <QDate>`, `#include <QPair>`, `#include <QTime>` to the includes block, then add after the `LastNextAppointment`/`lastAndNextAppointment` declarations:

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
  QVector<DuckEvent> upcoming; // up to 3, chronological
};

DaySummary computeDaySummary(const QVector<DuckEvent> &events,
                             const QVector<QPair<QDateTime, QDateTime>> &busyIntervals,
                             QTime workDayStart, QTime workDayEnd,
                             const QDate &selectedDate, qint64 nowMs,
                             int minFreeWindowMinutes);
```

- [ ] **Step 4: Implement `computeDaySummary`**

In `src/event_view/recurrence_utils.cpp`, add `#include <QSet>` to the includes, then add the implementation after `lastAndNextAppointment`:

```cpp
DaySummary computeDaySummary(const QVector<DuckEvent> &events,
                             const QVector<QPair<QDateTime, QDateTime>> &busyIntervals,
                             const QTime workDayStart, const QTime workDayEnd,
                             const QDate &selectedDate, const qint64 nowMs,
                             const int minFreeWindowMinutes) {
  DaySummary result;
  result.date = selectedDate;

  QVector<DuckEvent> qualifying;
  QSet<QString> namedClients;
  int unnamedClientCount = 0;
  for (const auto &event : events) {
    if (!event.is_work_event) {
      continue;
    }
    if (event.event_stat_id != 1 && event.event_stat_id != 2 && event.event_stat_id != 4) {
      continue; // only Scheduled/Completed/Confirmed count, same as lastAndNextAppointment
    }
    qualifying.append(event);

    const auto clientName = QString::fromStdString(event.client_name.value_or(""));
    if (!clientName.isEmpty()) {
      namedClients.insert(clientName);
    } else {
      ++unnamedClientCount;
    }

    if (event.start_date.has_value() && event.end_date.has_value() &&
        *event.end_date > *event.start_date) {
      result.busyMinutes += (*event.end_date - *event.start_date) / 60'000;
    }
  }

  result.sessionCount = static_cast<int>(qualifying.size());
  result.hasSessions = result.sessionCount > 0;
  result.clientCount = static_cast<int>(namedClients.size()) + unnamedClientCount;

  if (!result.hasSessions) {
    return result;
  }

  std::sort(qualifying.begin(), qualifying.end(),
            [](const DuckEvent &left, const DuckEvent &right) {
              return left.start_date.value_or(0) < right.start_date.value_or(0);
            });

  const bool validWorkHours = workDayStart.isValid() && workDayEnd.isValid() &&
                              workDayStart < workDayEnd;
  const auto dayEnd = QDateTime(selectedDate, workDayEnd, QTimeZone::systemTimeZone());
  const bool showFutureInfo = validWorkHours && nowMs < dayEnd.toMSecsSinceEpoch();

  if (!showFutureInfo) {
    for (const auto &event : qualifying) {
      result.upcoming.append(event);
      if (result.upcoming.size() >= 3) {
        break;
      }
    }
    return result;
  }

  for (const auto &event : qualifying) {
    if (event.start_date.has_value() && *event.start_date > nowMs) {
      result.upcoming.append(event);
      if (result.upcoming.size() >= 3) {
        break;
      }
    }
  }

  for (const auto &event : qualifying) {
    if (event.start_date.has_value() && *event.start_date > nowMs) {
      result.nextSession = event;
      break;
    }
  }

  const auto dayStart = QDateTime(selectedDate, workDayStart, QTimeZone::systemTimeZone());
  const auto now = QDateTime::fromMSecsSinceEpoch(nowMs, QTimeZone::systemTimeZone());

  auto sortedBusy = busyIntervals;
  std::sort(sortedBusy.begin(), sortedBusy.end(),
            [](const QPair<QDateTime, QDateTime> &left, const QPair<QDateTime, QDateTime> &right) {
              return left.first < right.first;
            });

  auto cursor = std::max(dayStart, now);
  for (const auto &interval : sortedBusy) {
    if (cursor >= dayEnd) {
      break;
    }
    if (interval.second <= cursor) {
      continue;
    }
    if (interval.first > cursor) {
      const auto gapEnd = std::min(interval.first, dayEnd);
      if (cursor.secsTo(gapEnd) / 60 >= minFreeWindowMinutes) {
        result.freeWindowStart = cursor;
        result.freeWindowEnd = gapEnd;
        break;
      }
    }
    cursor = std::max(cursor, interval.second);
  }
  if (!result.freeWindowStart.has_value() && cursor < dayEnd &&
      cursor.secsTo(dayEnd) / 60 >= minFreeWindowMinutes) {
    result.freeWindowStart = cursor;
    result.freeWindowEnd = dayEnd;
  }

  return result;
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build-release --target PsyClientManager_recurrence_tests -j"$(nproc)" && ctest --test-dir build-release -R RecurrenceUtilsTest --output-on-failure`
Expected: all `RecurrenceUtilsTest.ComputeDaySummary*` tests PASS, and the pre-existing `RecurrenceUtilsTest.*` tests still PASS.

- [ ] **Step 6: Commit**

```bash
git add src/event_view/recurrence_utils.h src/event_view/recurrence_utils.cpp test/recurrence_utils_tests.cpp
git commit -m "feat(calendar): add computeDaySummary for the day-summary panel"
```

---

### Task 2: Timeline "highlight" plumbing

**Files:**
- Modify: `src/event_view/event_item.h`
- Modify: `src/event_view/event_item.cpp`
- Modify: `src/event_view/event_view.h`
- Modify: `src/event_view/event_view.cpp`
- Modify: `src/timeline_widget/timeline_widget.h`
- Modify: `src/timeline_widget/timeline_widget.cpp`

**Interfaces:**
- Consumes: nothing new from Task 1.
- Produces: `QTimelineWidget::highlightEvent(int64_t eventId)` — consumed by Task 4's wiring of `DaySummaryWidget::eventHighlightRequested`.

No unit tests here — `QEventItem`/`QEventView`/`QTimelineWidget` are Qt graphics-scene widgets, and this codebase only unit-tests pure logic (see `test/recurrence_utils_tests.cpp` precedent). Verify by building and by the manual smoke test in Task 5.

- [ ] **Step 1: Add the highlight flag to `QEventItem`**

In `src/event_view/event_item.h`, add to the public section (near the other `set*` methods):

```cpp
  void setHighlighted(bool highlighted);
```

Add to the private section (near `mIsOnline`):

```cpp
  bool mIsHighlighted = false;
```

- [ ] **Step 2: Implement `setHighlighted` and paint the highlight border**

In `src/event_view/event_item.cpp`, add near the other simple setters (e.g. next to `setOnline`):

```cpp
void QEventItem::setHighlighted(const bool highlighted) {
  if (mIsHighlighted == highlighted) {
    return;
  }
  mIsHighlighted = highlighted;
  update();
}
```

In `paint()`, immediately before the final `painter->restore();` (the one that closes the whole function, currently the last line of the method), add:

```cpp
  if (mIsHighlighted) {
    QPen highlightPen(pcm::widgets::constants::kCalendarCurrentDayUnderlineColor, 3);
    painter->setPen(highlightPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(x - 1, -1, mSize.width() + 2, mSize.height() + 2, 6, 6);
  }
```

(`x` and `mSize` are already in scope at that point in `paint()` — `x` is the rect's horizontal offset computed earlier in the function, used for the fill/border drawing.)

- [ ] **Step 3: Add `highlightEvent` to `QEventView`**

In `src/event_view/event_view.h`, add to `public slots:` (after `onModelReset`):

```cpp
  void highlightEvent(int64_t eventId);
```

Add to the private section (near `mSelectedDay`):

```cpp
  int64_t mHighlightedEventId = -1;
```

In `src/event_view/event_view.cpp`, add the implementation (e.g. after `onModelReset`):

```cpp
void QEventView::highlightEvent(const int64_t eventId) {
  if (mHighlightedEventId >= 0) {
    if (auto *previous = mSceneItems.value(mHighlightedEventId)) {
      previous->setHighlighted(false);
    }
  }

  auto *item = mSceneItems.value(eventId);
  if (!item) {
    mHighlightedEventId = -1;
    return;
  }

  item->setHighlighted(true);
  mHighlightedEventId = eventId;
  centerOn(item);
}
```

- [ ] **Step 4: Forward from `QTimelineWidget`**

In `src/timeline_widget/timeline_widget.h`, add to `public slots:` (after `onSelectedDayChanged`):

```cpp
    void highlightEvent(int64_t eventId) const;
```

In `src/timeline_widget/timeline_widget.cpp`, add the implementation near `hasConflict`:

```cpp
void QTimelineWidget::highlightEvent(const int64_t eventId) const {
  if (mEventView) {
    mEventView->highlightEvent(eventId);
  }
}
```

(Check the existing `mEventView` member name in `timeline_widget.h` before writing this — it is the `QEventView *` member `QTimelineWidget` already owns and forwards other calls through, e.g. inside `onSelectedDayChanged`.)

- [ ] **Step 5: Build**

Run: `cmake --build build-release --target PsyClientManager_event_view PsyClientManager_timeline_widget -j"$(nproc)"`
Expected: builds cleanly.

- [ ] **Step 6: Commit**

```bash
git add src/event_view/event_item.h src/event_view/event_item.cpp src/event_view/event_view.h src/event_view/event_view.cpp src/timeline_widget/timeline_widget.h src/timeline_widget/timeline_widget.cpp
git commit -m "feat(calendar): add QTimelineWidget::highlightEvent for the day-summary mini-list"
```

---

### Task 3: `DaySummaryWidget`

**Files:**
- Create: `src/pages/event_info_page/day_summary_widget.h`
- Create: `src/pages/event_info_page/day_summary_widget.cpp`
- Modify: `src/pages/event_info_page/CMakeLists.txt`

**Interfaces:**
- Consumes: `pcm::recurrence::DaySummary` (Task 1).
- Produces:
  ```cpp
  class DaySummaryWidget final : public QWidget {
  public:
    explicit DaySummaryWidget(QWidget *parent = nullptr);
  public slots:
    void setSummary(const pcm::recurrence::DaySummary &summary);
  signals:
    void eventHighlightRequested(int64_t eventId);
  };
  ```
  Consumed by Task 4 (`QEventInfoPage`).

- [ ] **Step 1: Create the header**

Create `src/pages/event_info_page/day_summary_widget.h`:

```cpp
#pragma once

#include "recurrence_utils.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

class DaySummaryWidget final : public QWidget {
  Q_OBJECT

public:
  explicit DaySummaryWidget(QWidget *parent = nullptr);

public slots:
  void setSummary(const pcm::recurrence::DaySummary &summary);

signals:
  void eventHighlightRequested(int64_t eventId);

private:
  void clearMiniList();

  QLabel *mDateLabel = nullptr;
  QLabel *mCountsLabel = nullptr;
  QLabel *mBusyLabel = nullptr;
  QLabel *mNextSessionLabel = nullptr;
  QLabel *mFreeWindowLabel = nullptr;
  QVBoxLayout *mMiniListLayout = nullptr;
};
```

- [ ] **Step 2: Create the implementation**

Create `src/pages/event_info_page/day_summary_widget.cpp`:

```cpp
#include "day_summary_widget.h"

#include <QDateTime>
#include <QFont>
#include <QLocale>
#include <QPushButton>
#include <QTimeZone>

DaySummaryWidget::DaySummaryWidget(QWidget *parent) : QWidget(parent) {
  setObjectName("daySummaryCard");
  setStyleSheet(
      "#daySummaryCard {"
      " background: rgba(255, 255, 255, 0.05);"
      " border: 1px solid rgba(255, 255, 255, 0.10);"
      " border-radius: 14px;"
      "}");

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(14, 12, 14, 12);
  layout->setSpacing(4);

  mDateLabel = new QLabel(this);
  QFont dateFont = mDateLabel->font();
  dateFont.setBold(true);
  mDateLabel->setFont(dateFont);

  mCountsLabel = new QLabel(this);
  mCountsLabel->setStyleSheet("color: rgba(255, 255, 255, 0.75);");
  mBusyLabel = new QLabel(this);
  mBusyLabel->setStyleSheet("color: rgba(255, 255, 255, 0.65);");
  mNextSessionLabel = new QLabel(this);
  mNextSessionLabel->setStyleSheet("color: rgba(255, 255, 255, 0.65);");
  mFreeWindowLabel = new QLabel(this);
  mFreeWindowLabel->setStyleSheet("color: rgba(255, 255, 255, 0.65);");

  layout->addWidget(mDateLabel);
  layout->addWidget(mCountsLabel);
  layout->addWidget(mBusyLabel);
  layout->addWidget(mNextSessionLabel);
  layout->addWidget(mFreeWindowLabel);

  mMiniListLayout = new QVBoxLayout();
  mMiniListLayout->setContentsMargins(0, 6, 0, 0);
  mMiniListLayout->setSpacing(4);
  layout->addLayout(mMiniListLayout);
}

void DaySummaryWidget::clearMiniList() {
  while (auto *item = mMiniListLayout->takeAt(0)) {
    delete item->widget();
    delete item;
  }
}

void DaySummaryWidget::setSummary(const pcm::recurrence::DaySummary &summary) {
  clearMiniList();

  mDateLabel->setText(QLocale().toString(summary.date, QLocale::LongFormat));

  if (!summary.hasSessions) {
    mCountsLabel->setText(tr("Free all day"));
    mBusyLabel->clear();
    mBusyLabel->setVisible(false);
    mNextSessionLabel->clear();
    mNextSessionLabel->setVisible(false);
    mFreeWindowLabel->clear();
    mFreeWindowLabel->setVisible(false);
    return;
  }

  mCountsLabel->setText(tr("%n session(s) · %1 client(s)", "", summary.sessionCount)
                            .arg(summary.clientCount));

  const auto busyHours = summary.busyMinutes / 60;
  const auto busyMinutesRemainder = summary.busyMinutes % 60;
  mBusyLabel->setText(tr("Busy: %1h %2m").arg(busyHours).arg(busyMinutesRemainder));
  mBusyLabel->setVisible(true);

  if (summary.nextSession.has_value() && summary.nextSession->start_date.has_value()) {
    const auto nextAt = QDateTime::fromMSecsSinceEpoch(*summary.nextSession->start_date,
                                                        QTimeZone::systemTimeZone());
    mNextSessionLabel->setText(tr("Next session: %1").arg(nextAt.toString("HH:mm")));
    mNextSessionLabel->setVisible(true);
  } else {
    mNextSessionLabel->clear();
    mNextSessionLabel->setVisible(false);
  }

  if (summary.freeWindowStart.has_value() && summary.freeWindowEnd.has_value()) {
    mFreeWindowLabel->setText(tr("Nearest free window: %1-%2")
                                  .arg(summary.freeWindowStart->toString("HH:mm"),
                                       summary.freeWindowEnd->toString("HH:mm")));
    mFreeWindowLabel->setVisible(true);
  } else {
    mFreeWindowLabel->clear();
    mFreeWindowLabel->setVisible(false);
  }

  for (const auto &event : summary.upcoming) {
    if (!event.start_date.has_value()) {
      continue;
    }
    const auto startAt =
        QDateTime::fromMSecsSinceEpoch(*event.start_date, QTimeZone::systemTimeZone());
    const auto clientName =
        QString::fromStdString(event.client_name.value_or(std::string{}));
    auto *row = new QPushButton(this);
    row->setFlat(true);
    row->setCursor(Qt::PointingHandCursor);
    row->setText(clientName.isEmpty()
                     ? startAt.toString("HH:mm")
                     : QString("%1  %2").arg(startAt.toString("HH:mm"), clientName));
    row->setStyleSheet(
        "QPushButton { text-align: left; color: rgba(255, 255, 255, 0.90); "
        "background: transparent; border: none; padding: 2px 0px; }");

    const auto eventId = event.id;
    connect(row, &QPushButton::clicked, this,
            [this, eventId]() { emit eventHighlightRequested(eventId); });

    mMiniListLayout->addWidget(row);
  }
}
```

- [ ] **Step 3: Wire the new files into the build**

In `src/pages/event_info_page/CMakeLists.txt`, add `day_summary_widget.cpp` and `day_summary_widget.h` to the `qt_add_library(${TARGET_NAME} STATIC ...)` sources list (alongside `event_info.cpp`, `qevent_details_widget.cpp`, `qevent_details_widget.h`).

- [ ] **Step 4: Build**

Run: `cmake --build build-release --target PsyClientManager_event_page -j"$(nproc)"`
Expected: builds cleanly.

- [ ] **Step 5: Commit**

```bash
git add src/pages/event_info_page/day_summary_widget.h src/pages/event_info_page/day_summary_widget.cpp src/pages/event_info_page/CMakeLists.txt
git commit -m "feat(calendar): add DaySummaryWidget"
```

---

### Task 4: Wire `DaySummaryWidget` into `QEventInfoPage`

**Files:**
- Modify: `ui/pages/eventinfo.ui`
- Modify: `src/pages/event_info_page/event_info.h`
- Modify: `src/pages/event_info_page/event_info.cpp`

**Interfaces:**
- Consumes: `DaySummaryWidget` (Task 3), `pcm::recurrence::computeDaySummary` (Task 1), `QTimelineWidget::highlightEvent` (Task 2), existing `QEventInfoPage::currentBusyIntervals()`, `pcm::app_settings::workDayStart()`/`workDayEnd()`/`defaultSessionDurationMinutes()`.
- Produces: nothing new consumed elsewhere — this is the final integration task.

- [ ] **Step 1: Trim the dead layout items in `eventinfo.ui`**

In `ui/pages/eventinfo.ui`, remove the `calendarBottomSpacer` `<item>` block (the `<spacer name="calendarBottomSpacer">...</spacer>` item) and the `label` `<item>` block (the `<widget class="QLabel" name="label">...</widget>` item) from `verticalLayout`. After this change, `verticalLayout` should contain exactly one `<item>`: `calendarCard`.

- [ ] **Step 2: Add the member and method declarations**

In `src/pages/event_info_page/event_info.h`:
- Add `#include "day_summary_widget.h"` to the includes.
- Add `DaySummaryWidget *mDaySummaryWidget = nullptr;` to the private member section (near `mQuickSlotsWidget`).
- Add `void refreshDaySummary() const;` to the private method section (near `refreshQuickSlots`).
- Add `void onDaySummaryEventHighlightRequested(int64_t eventId);` to `private slots:` (near `onCalendarClicked`).

- [ ] **Step 3: Construct and insert the widget**

In `src/pages/event_info_page/event_info.cpp`'s constructor, replace:

```cpp
  mQuickSlotsWidget = new QuickSlotsWidget(this);
  mUi->verticalLayout->insertWidget(2, mQuickSlotsWidget);
  mUi->label->hide();
```

with:

```cpp
  mDaySummaryWidget = new DaySummaryWidget(this);
  mUi->verticalLayout->addWidget(mDaySummaryWidget);

  mQuickSlotsWidget = new QuickSlotsWidget(this);
  mUi->verticalLayout->addWidget(mQuickSlotsWidget);
```

(`verticalLayout` now only has `calendarCard` from the `.ui` file after Step 1, so appending is equivalent to — and simpler than — the old index-based insert.)

- [ ] **Step 4: Connect the highlight signal**

In `connectSignals()`, add:

```cpp
  connect(mDaySummaryWidget, &DaySummaryWidget::eventHighlightRequested, this,
          &QEventInfoPage::onDaySummaryEventHighlightRequested);
```

- [ ] **Step 5: Implement `refreshDaySummary` and the highlight slot**

Add near `refreshQuickSlots`:

```cpp
void QEventInfoPage::refreshDaySummary() const {
  if (!mDaySummaryWidget || !mTimelineWidget) {
    return;
  }
  const auto nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
  const auto summary = pcm::recurrence::computeDaySummary(
      mTimelineWidget->events(), currentBusyIntervals(), pcm::app_settings::workDayStart(),
      pcm::app_settings::workDayEnd(), mSelectedDate, nowMs,
      pcm::app_settings::defaultSessionDurationMinutes());
  mDaySummaryWidget->setSummary(summary);
}

void QEventInfoPage::onDaySummaryEventHighlightRequested(const int64_t eventId) {
  if (mTimelineWidget) {
    mTimelineWidget->highlightEvent(eventId);
  }
}
```

- [ ] **Step 6: Call `refreshDaySummary` everywhere `refreshQuickSlots` is already called**

Add a `refreshDaySummary();` call immediately after each existing `refreshQuickSlots();` call in `event_info.cpp`:
- `initDefaultStates()`
- `onCalendarClicked(const QDate &date)`
- `onTimelineEventDeleteRequested(const int64_t eventId)` — all three call sites inside it (whole-series branch, future-occurrences branch, and the final one after `mTimelineWidget->removeEvent(eventId)`)
- `onEventSaved(QEventItem *event)`
- `refreshAppearance()`

- [ ] **Step 7: Build**

Run: `cmake --build build-release -j"$(nproc)"`
Expected: builds cleanly, no warnings about the removed `.ui` items (they're gone from `ui_eventinfo.h` too since it's regenerated).

- [ ] **Step 8: Commit**

```bash
git add ui/pages/eventinfo.ui src/pages/event_info_page/event_info.h src/pages/event_info_page/event_info.cpp
git commit -m "feat(calendar): wire DaySummaryWidget into the Calendar page left column"
```

---

### Task 5: Version bump, changelog, full verification

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/app/application.cpp`
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Bump the version**

In `CMakeLists.txt`, change `project(PsyClientManager VERSION 0.1.19 ...)` to `VERSION 0.1.20`.
In `src/app/application.cpp`, change `app.setApplicationVersion("0.1.19");` to `app.setApplicationVersion("0.1.20");`.

- [ ] **Step 2: Add the changelog entry**

In `CHANGELOG.md`, add above the `## [0.1.19]` entry:

```markdown
## [0.1.20] - 2026-07-31

### Added

- Day summary panel on the Calendar page: session/client counts, busy
  time, next session, and the nearest free window for the selected day,
  plus a mini list of upcoming sessions that highlights the matching
  card on the timeline when clicked. Closes the empty gap between the
  calendar and Quick Slots.
```

- [ ] **Step 3: Full rebuild and test suite**

Run: `cmake --build build-release -j"$(nproc)"`
Expected: builds cleanly.

Run: `ctest --test-dir build-release --output-on-failure`
Expected: 100% tests passed (including the new `ComputeDaySummary*` tests from Task 1).

- [ ] **Step 4: Manual smoke test**

Run: `pgrep -af PsyClientManager` first — kill any stragglers (`kill -9 <pid>`) to avoid a false DB-lock crash, per this project's known orphaned-process gotcha.

Launch `./build-release/PsyClientManager` in the background, confirm:
- The Calendar page's left column shows Calendar → day summary → Quick Slots with no dead gap.
- Selecting today with existing sessions shows correct counts/busy time/next session/free window.
- Selecting a day with no Work sessions shows the "Free all day" empty state.
- Selecting a past day hides "next session" and "free window" but still shows counts and the mini-list.
- Clicking a mini-list row highlights the matching card on the timeline (visible accent border) and scrolls it into view, without opening the edit dialog.
- Directly clicking a timeline card still opens the edit dialog as before (regression check on the existing path).

Kill the process cleanly afterward (`kill <pid>`, verify with `pgrep -af PsyClientManager`).

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/app/application.cpp CHANGELOG.md
git commit -m "chore: bump version to 0.1.20"
```
