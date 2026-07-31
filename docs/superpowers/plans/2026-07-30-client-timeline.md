# Unified Client Timeline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Evolve `ClientNotesPage` (built in #48) into a unified client timeline that interleaves session (event) entries with notes, adds a last/next-appointment summary to both the timeline header and the client card, and adds an All/Sessions/Notes type filter.

**Architecture:** Two new read-only `Database` queries fetch a client's materialized events (unbounded) and active series (bounded range). A new `pcm::recurrence::eventsForClient` composes those with the existing occurrence-expansion helpers (`occurrences`/`buildVirtualOccurrence`, already proven in `QTimelineModel::loadEventsForDay`) into one sorted `QVector<DuckEvent>`, windowed ±3 months for virtual occurrences only. A new pure `pcm::recurrence::lastAndNextAppointment` reduces that vector to the two appointments closest to "now". `ClientNotesPage` and `QClientInfoCardPage` both consume these two functions independently — no new page, no new nav entry.

**Tech Stack:** C++20, Qt6 Widgets, DuckDB (C++ client), `oclero::qlementine::SegmentedControl`, GoogleTest.

## Global Constraints

- No database schema changes in this plan — only new read queries against existing tables. (Per AGENTS.md, schema changes require a migration + round-trip test; not applicable here.)
- Every task that touches `client_notes_page` or `detail_client_info_page` CMake targets must add `${PROJECT_NAME}_event_view` as a dependency (see Task 5 and Task 6) — these are new module dependencies, confirmed as architecturally justified in the design doc.
- **Never use a plain `QComboBox` in a widget constructed before `mMainWindow->show()`** — it triggers a reentrancy crash in `oclero::qlementine`'s combobox popup construction (root-caused and fixed in #48). Use `oclero::qlementine::SegmentedControl` instead, exactly as `qevent_details_widget.cpp` and `analytics_page.cpp` already do.
- Every MR needs an issue (#30), a branch (`feat/30-client-timeline`, already created and rebased onto `feat/48-notes-journal`), a version bump, and a CHANGELOG entry (Task 7).
- Design doc: `docs/superpowers/specs/2026-07-30-client-timeline-design.md`.

---

### Task 1: `Database::get_events_for_client`

**Files:**
- Modify: `src/database/constants.hpp` (add query constant near `kSelectClientNotesQuery`, line ~392)
- Modify: `src/database/database.h:79` (add declaration after `add_event_client`)
- Modify: `src/database/database.cpp` (add definition near `get_client_notes`, line ~722)
- Test: `test/database_tests.cpp`

**Interfaces:**
- Produces: `std::vector<DuckEvent> Database::get_events_for_client(int64_t client_id)` — all materialized events linked to the client via `EventClient`, ordered by `start_date` ascending. Unbounded (no date range).

- [ ] **Step 1: Write the failing test**

Append to `test/database_tests.cpp`:

```cpp
TEST(DatabaseTest, GetEventsForClientReturnsOnlyLinkedEvents) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_events_for_client")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  DuckClient clientA;
  clientA.name = std::string{"Alice"};
  clientA.last_name = std::string{"A"};
  const auto clientAId = db.add_client(clientA);
  ASSERT_GT(clientAId, 0);

  DuckClient clientB;
  clientB.name = std::string{"Bob"};
  clientB.last_name = std::string{"B"};
  const auto clientBId = db.add_client(clientB);
  ASSERT_GT(clientBId, 0);

  DuckEvent eventForA;
  eventForA.name = std::string{"Session with Alice"};
  eventForA.start_date = 1730000000000;
  eventForA.end_date = 1730003600000;
  const auto eventForAId = db.add_event(eventForA);
  ASSERT_GT(eventForAId, 0);
  ASSERT_GT(db.add_event_client(eventForAId, clientAId), 0);

  DuckEvent eventForB;
  eventForB.name = std::string{"Session with Bob"};
  eventForB.start_date = 1730100000000;
  eventForB.end_date = 1730103600000;
  const auto eventForBId = db.add_event(eventForB);
  ASSERT_GT(eventForBId, 0);
  ASSERT_GT(db.add_event_client(eventForBId, clientBId), 0);

  const auto eventsForA = db.get_events_for_client(clientAId);
  ASSERT_EQ(eventsForA.size(), 1);
  EXPECT_EQ(eventsForA.front().id, eventForAId);
  EXPECT_EQ(eventsForA.front().name.value_or(""), "Session with Alice");

  db_dir.remove(true);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-release --target PsyClientManager_database_tests && ./build-release/test/PsyClientManager_database_tests --gtest_filter=DatabaseTest.GetEventsForClientReturnsOnlyLinkedEvents`
Expected: FAIL to compile — `get_events_for_client` is not a member of `Database`.

- [ ] **Step 3: Add the query constant**

In `src/database/constants.hpp`, add directly after `kSelectClientNotesQuery` (~line 397):

```cpp
constexpr auto kSelectEventsForClientQuery = R"duckdb(
SELECT e.* FROM Event e
JOIN EventClient ec ON ec.event_id = e.id
WHERE ec.client_id = $1
ORDER BY e.start_date ASC
)duckdb";
```

- [ ] **Step 4: Declare the method**

In `src/database/database.h`, directly after the `add_event_client` declaration (line 79):

```cpp
  std::vector<DuckEvent> get_events_for_client(int64_t client_id);
```

- [ ] **Step 5: Implement the method**

In `src/database/database.cpp`, directly after `Database::get_client_notes` (~line 743):

```cpp
std::vector<DuckEvent> Database::get_events_for_client(const int64_t client_id) {
  if (client_id <= 0) {
    return {};
  }

  duckdb::Connection conn(*mDb);
  auto result = executePrepared(
      conn, constance::kSelectEventsForClientQuery, {duckdb::Value::BIGINT(client_id)});
  if (!result || result->HasError()) {
    PLOG_ERROR << "Failed to fetch events for client_id=" << client_id << ": "
               << (result ? result->GetError() : "prepare failed");
    return {};
  }

  std::vector<DuckEvent> events;
  while (auto chunk = result->Fetch()) {
    for (duckdb::idx_t i = 0; i < chunk->size(); ++i) {
      events.emplace_back(*chunk, i);
    }
  }

  return events;
}
```

- [ ] **Step 6: Run test to verify it passes**

Run: `cmake --build build-release --target PsyClientManager_database_tests && ./build-release/test/PsyClientManager_database_tests --gtest_filter=DatabaseTest.GetEventsForClientReturnsOnlyLinkedEvents`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add src/database/constants.hpp src/database/database.h src/database/database.cpp test/database_tests.cpp
git commit -m "feat(database): add get_events_for_client for the client timeline"
```

---

### Task 2: `Database::get_event_series_for_client_and_range`

**Files:**
- Modify: `src/database/constants.hpp` (add query constant near `kSelectEventSeriesForRangeQuery`, line ~248)
- Modify: `src/database/database.h:63` (add declaration after `get_event_series_for_range`)
- Modify: `src/database/database.cpp` (add definition near `get_event_series_for_range`, ~line 416)
- Test: `test/database_tests.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: `std::vector<DuckEventSeries> Database::get_event_series_for_client_and_range(int64_t client_id, int64_t range_start_ms, int64_t range_end_ms)` — active series belonging to the client that overlap `[range_start_ms, range_end_ms]`.

- [ ] **Step 1: Write the failing test**

Append to `test/database_tests.cpp`:

```cpp
TEST(DatabaseTest, GetEventSeriesForClientAndRangeFiltersByClientAndRange) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_series_for_client")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  DuckClient client;
  client.name = std::string{"Carol"};
  client.last_name = std::string{"C"};
  const auto clientId = db.add_client(client);
  ASSERT_GT(clientId, 0);

  DuckClient otherClient;
  otherClient.name = std::string{"Dave"};
  otherClient.last_name = std::string{"D"};
  const auto otherClientId = db.add_client(otherClient);
  ASSERT_GT(otherClientId, 0);

  DuckEventSeries series;
  series.name = std::string{"Weekly with Carol"};
  series.client_id = clientId;
  series.start_date = 1730000000000;
  series.duration = 3600;
  series.recurrence_rule = "FREQ=WEEKLY;INTERVAL=1";
  const auto seriesId = db.add_event_series(series);
  ASSERT_GT(seriesId, 0);

  DuckEventSeries otherSeries;
  otherSeries.name = std::string{"Weekly with Dave"};
  otherSeries.client_id = otherClientId;
  otherSeries.start_date = 1730000000000;
  otherSeries.duration = 3600;
  otherSeries.recurrence_rule = "FREQ=WEEKLY;INTERVAL=1";
  ASSERT_GT(db.add_event_series(otherSeries), 0);

  const auto inRange =
      db.get_event_series_for_client_and_range(clientId, 1729000000000, 1731000000000);
  ASSERT_EQ(inRange.size(), 1);
  EXPECT_EQ(inRange.front().id, seriesId);

  const auto outOfRange =
      db.get_event_series_for_client_and_range(clientId, 1600000000000, 1700000000000);
  EXPECT_TRUE(outOfRange.empty());

  const auto forOtherClient =
      db.get_event_series_for_client_and_range(otherClientId, 1729000000000, 1731000000000);
  ASSERT_EQ(forOtherClient.size(), 1);
  EXPECT_NE(forOtherClient.front().id, seriesId);

  db_dir.remove(true);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-release --target PsyClientManager_database_tests && ./build-release/test/PsyClientManager_database_tests --gtest_filter=DatabaseTest.GetEventSeriesForClientAndRangeFiltersByClientAndRange`
Expected: FAIL to compile — `get_event_series_for_client_and_range` is not a member of `Database`.

- [ ] **Step 3: Add the query constant**

In `src/database/constants.hpp`, directly after `kSelectEventSeriesForRangeQuery` (~line 248):

```cpp
constexpr auto kSelectEventSeriesForClientAndRangeQuery = R"duckdb(
SELECT * FROM EventSeries
WHERE client_id = $1
  AND active = TRUE
  AND start_date <= $2
  AND (recurrence_until IS NULL OR recurrence_until >= $3)
)duckdb";
```

- [ ] **Step 4: Declare the method**

In `src/database/database.h`, directly after `get_event_series_for_range` (line 64):

```cpp
  std::vector<DuckEventSeries> get_event_series_for_client_and_range(
      int64_t client_id, int64_t range_start_ms, int64_t range_end_ms);
```

- [ ] **Step 5: Implement the method**

In `src/database/database.cpp`, directly after `Database::get_event_series_for_range` (~line 416), mirroring its exact binding order (`start_date <= end_ms`, `recurrence_until >= start_ms`):

```cpp
std::vector<DuckEventSeries> Database::get_event_series_for_client_and_range(
    const int64_t client_id, const int64_t range_start_ms, const int64_t range_end_ms) {
  if (client_id <= 0) {
    return {};
  }

  duckdb::Connection conn(*mDb);
  auto result = executePrepared(
      conn, constance::kSelectEventSeriesForClientAndRangeQuery,
      {duckdb::Value::BIGINT(client_id),
       db_utils::toDuckTimestamp(std::make_optional(range_end_ms * 1000)),
       db_utils::toDuckTimestamp(std::make_optional(range_start_ms * 1000))});

  if (!result || result->HasError()) {
    PLOG_ERROR << "Failed to fetch event series for client_id=" << client_id << ": "
               << (result ? result->GetError() : "prepare failed");
    return {};
  }

  std::vector<DuckEventSeries> series;
  while (auto chunk = result->Fetch()) {
    for (duckdb::idx_t i = 0; i < chunk->size(); ++i) {
      series.emplace_back(*chunk, i);
    }
  }

  return series;
}
```

- [ ] **Step 6: Run test to verify it passes**

Run: `cmake --build build-release --target PsyClientManager_database_tests && ./build-release/test/PsyClientManager_database_tests --gtest_filter=DatabaseTest.GetEventSeriesForClientAndRangeFiltersByClientAndRange`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add src/database/constants.hpp src/database/database.h src/database/database.cpp test/database_tests.cpp
git commit -m "feat(database): add get_event_series_for_client_and_range for the client timeline"
```

---

### Task 3: `pcm::recurrence::eventsForClient` + `lastAndNextAppointment`

**Files:**
- Modify: `src/event_view/recurrence_utils.h`
- Modify: `src/event_view/recurrence_utils.cpp`
- Create: `test/recurrence_utils_tests.cpp`
- Modify: `test/CMakeLists.txt` (register new test executable)

**Interfaces:**
- Consumes: `Database::get_events_for_client`, `Database::get_event_series_for_client_and_range`, `Database::get_event_series_exceptions_for_range`, `Database::get_materialized_occurrence_starts_for_series` (all existing/Task 1/Task 2), `pcm::recurrence::occurrences`, `pcm::recurrence::buildVirtualOccurrence` (existing).
- Produces:
  ```cpp
  QVector<DuckEvent> eventsForClient(pcm::database::Database &db, int64_t clientId,
                                     const QDateTime &virtualWindowStart,
                                     const QDateTime &virtualWindowEnd);

  struct LastNextAppointment {
    std::optional<DuckEvent> last;
    std::optional<DuckEvent> next;
  };
  LastNextAppointment lastAndNextAppointment(const QVector<DuckEvent> &events, qint64 nowMs);
  ```
  Both consumed by Task 5 (`ClientNotesPage`) and Task 6 (`QClientInfoCardPage`).

- [ ] **Step 1: Write the failing tests**

Create `test/recurrence_utils_tests.cpp`:

```cpp
#include <Poco/File.h>
#include <Poco/Path.h>
#include <gtest/gtest.h>

#include "config.h"
#include "database.h"
#include "recurrence_utils.h"

namespace {
DuckEvent eventAt(const int64_t id, const int64_t startMs, const int64_t eventStatId = 1) {
  DuckEvent event;
  event.id = id;
  event.start_date = startMs;
  event.end_date = startMs + 3'600'000;
  event.event_stat_id = eventStatId;
  return event;
}
} // namespace

TEST(RecurrenceUtilsTest, EventsForClientCombinesMaterializedAndVirtualOccurrences) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_events_for_client_recurrence")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  DuckClient client;
  client.name = std::string{"Eve"};
  client.last_name = std::string{"E"};
  const auto clientId = db.add_client(client);
  ASSERT_GT(clientId, 0);

  const auto windowStart = QDateTime::currentDateTime().addMonths(-3);
  const auto windowEnd = QDateTime::currentDateTime().addMonths(3);

  DuckEvent standalone;
  standalone.name = std::string{"One-off session"};
  standalone.start_date = QDateTime::currentDateTime().addDays(1).toUTC().toMSecsSinceEpoch();
  standalone.end_date = *standalone.start_date + 3'600'000;
  const auto standaloneId = db.add_event(standalone);
  ASSERT_GT(standaloneId, 0);
  ASSERT_GT(db.add_event_client(standaloneId, clientId), 0);

  DuckEventSeries series;
  series.name = std::string{"Weekly with Eve"};
  series.client_id = clientId;
  series.start_date = QDateTime::currentDateTime().addDays(2).toUTC().toMSecsSinceEpoch();
  series.duration = 3600;
  series.recurrence_rule =
      pcm::recurrence::weeklyRuleForDate(QDateTime::currentDateTime().addDays(2).date()).toStdString();
  const auto seriesId = db.add_event_series(series);
  ASSERT_GT(seriesId, 0);

  const auto events =
      pcm::recurrence::eventsForClient(db, clientId, windowStart, windowEnd);

  const auto standaloneCount =
      std::count_if(events.begin(), events.end(),
                    [standaloneId](const DuckEvent &e) { return e.id == standaloneId; });
  EXPECT_EQ(standaloneCount, 1);

  const auto virtualCount =
      std::count_if(events.begin(), events.end(),
                    [seriesId](const DuckEvent &e) {
                      return e.is_virtual_occurrence && e.series_id == seriesId;
                    });
  EXPECT_GE(virtualCount, 1);

  db_dir.remove(true);
}

TEST(RecurrenceUtilsTest, EventsForClientSkipsMaterializedOccurrenceOfSameSeries) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_events_for_client_materialized")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  DuckClient client;
  client.name = std::string{"Frank"};
  client.last_name = std::string{"F"};
  const auto clientId = db.add_client(client);
  ASSERT_GT(clientId, 0);

  const auto seriesStartLocal = QDateTime::currentDateTime().addDays(1);
  DuckEventSeries series;
  series.name = std::string{"Weekly with Frank"};
  series.client_id = clientId;
  series.start_date = seriesStartLocal.toUTC().toMSecsSinceEpoch();
  series.duration = 3600;
  series.recurrence_rule =
      pcm::recurrence::weeklyRuleForDate(seriesStartLocal.date()).toStdString();
  const auto seriesId = db.add_event_series(series);
  ASSERT_GT(seriesId, 0);

  DuckEvent materialized;
  materialized.name = std::string{"Weekly with Frank"};
  materialized.start_date = seriesStartLocal.toUTC().toMSecsSinceEpoch();
  materialized.end_date = *materialized.start_date + 3'600'000;
  materialized.series_id = seriesId;
  materialized.original_occurrence_start = materialized.start_date;
  const auto materializedId = db.add_event(materialized);
  ASSERT_GT(materializedId, 0);
  ASSERT_GT(db.add_event_client(materializedId, clientId), 0);

  const auto windowStart = QDateTime::currentDateTime().addMonths(-3);
  const auto windowEnd = QDateTime::currentDateTime().addMonths(3);
  const auto events =
      pcm::recurrence::eventsForClient(db, clientId, windowStart, windowEnd);

  const auto occurrencesAtSeriesStart =
      std::count_if(events.begin(), events.end(), [&](const DuckEvent &e) {
        return e.start_date.has_value() &&
               std::abs(*e.start_date - *materialized.start_date) < 1000;
      });
  EXPECT_EQ(occurrencesAtSeriesStart, 1);

  db_dir.remove(true);
}

TEST(RecurrenceUtilsTest, LastAndNextAppointmentPastOnlyLeavesNextEmpty) {
  const QVector<DuckEvent> events{eventAt(1, 1'000)};
  const auto result = pcm::recurrence::lastAndNextAppointment(events, 2'000);
  ASSERT_TRUE(result.last.has_value());
  EXPECT_EQ(result.last->id, 1);
  EXPECT_FALSE(result.next.has_value());
}

TEST(RecurrenceUtilsTest, LastAndNextAppointmentFutureOnlyLeavesLastEmpty) {
  const QVector<DuckEvent> events{eventAt(1, 3'000)};
  const auto result = pcm::recurrence::lastAndNextAppointment(events, 2'000);
  EXPECT_FALSE(result.last.has_value());
  ASSERT_TRUE(result.next.has_value());
  EXPECT_EQ(result.next->id, 1);
}

TEST(RecurrenceUtilsTest, LastAndNextAppointmentSkipsCanceledEvents) {
  const QVector<DuckEvent> events{
      eventAt(1, 2'500, /*event_stat_id=*/3), // canceled, closer to now
      eventAt(2, 3'000, /*event_stat_id=*/1), // scheduled, farther
  };
  const auto result = pcm::recurrence::lastAndNextAppointment(events, 2'000);
  ASSERT_TRUE(result.next.has_value());
  EXPECT_EQ(result.next->id, 2);
}
```

- [ ] **Step 2: Register the new test executable**

In `test/CMakeLists.txt`, directly after the "Schedule conflict tests" block (~line 40):

```cmake
# Recurrence utils tests
add_executable(PsyClientManager_recurrence_tests recurrence_utils_tests.cpp)
target_link_libraries(PsyClientManager_recurrence_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    PsyClientManager_config
    PsyClientManager_database
    PsyClientManager_event_view
    Poco::Foundation
)
gtest_discover_tests(PsyClientManager_recurrence_tests)
```

- [ ] **Step 3: Run tests to verify they fail**

Run: `cmake --build build-release --target PsyClientManager_recurrence_tests`
Expected: FAIL to compile — `eventsForClient` and `lastAndNextAppointment` are not declared in `pcm::recurrence`.

- [ ] **Step 4: Declare the new functions**

In `src/event_view/recurrence_utils.h`, append inside `namespace pcm::recurrence` after `buildVirtualOccurrence`:

```cpp
QVector<DuckEvent> eventsForClient(pcm::database::Database &db, int64_t clientId,
                                   const QDateTime &virtualWindowStart,
                                   const QDateTime &virtualWindowEnd);

struct LastNextAppointment {
  std::optional<DuckEvent> last;
  std::optional<DuckEvent> next;
};

LastNextAppointment lastAndNextAppointment(const QVector<DuckEvent> &events, qint64 nowMs);
```

Add `#include <optional>` to the top of `recurrence_utils.h` (not currently included).

- [ ] **Step 5: Implement `eventsForClient`**

In `src/event_view/recurrence_utils.cpp`, append inside `namespace pcm::recurrence` after `buildVirtualOccurrence`, mirroring the composition in `QTimelineModel::loadEventsForDay` (`src/event_view/qtimeline_model.cpp:95-134`) scoped per-client instead of per-day:

```cpp
QVector<DuckEvent> eventsForClient(pcm::database::Database &db, const int64_t clientId,
                                   const QDateTime &virtualWindowStart,
                                   const QDateTime &virtualWindowEnd) {
  QVector<DuckEvent> result;

  const auto materialized = db.get_events_for_client(clientId);
  for (const auto &event : materialized) {
    result.append(event);
  }

  if (!virtualWindowStart.isValid() || !virtualWindowEnd.isValid()) {
    std::sort(result.begin(), result.end(), [](const DuckEvent &left, const DuckEvent &right) {
      return left.start_date.value_or(0) < right.start_date.value_or(0);
    });
    return result;
  }

  const auto windowStartMs = virtualWindowStart.toUTC().toMSecsSinceEpoch();
  const auto windowEndMs = virtualWindowEnd.toUTC().toMSecsSinceEpoch();
  const auto exceptions = db.get_event_series_exceptions_for_range(windowStartMs, windowEndMs);
  auto seriesList =
      db.get_event_series_for_client_and_range(clientId, windowStartMs, windowEndMs);
  for (const auto &series : seriesList) {
    const auto materializedStarts = db.get_materialized_occurrence_starts_for_series(series.id);
    const auto occurrenceList = occurrences(series, virtualWindowStart, virtualWindowEnd);
    for (const auto &occurrence : occurrenceList) {
      const auto occurrenceStartMs = occurrence.toUTC().toMSecsSinceEpoch();
      if (exceptions.contains({series.id, occurrenceStartMs}) ||
          materializedStarts.contains(occurrenceStartMs)) {
        continue;
      }
      const auto virtualId =
          -(series.id * 1'000'000LL + static_cast<int64_t>(occurrence.date().toJulianDay()));
      result.append(buildVirtualOccurrence(series, occurrence, virtualId));
    }
  }

  std::sort(result.begin(), result.end(), [](const DuckEvent &left, const DuckEvent &right) {
    return left.start_date.value_or(0) < right.start_date.value_or(0);
  });

  return result;
}
```

- [ ] **Step 6: Implement `lastAndNextAppointment`**

Append directly after `eventsForClient`:

```cpp
LastNextAppointment lastAndNextAppointment(const QVector<DuckEvent> &events, const qint64 nowMs) {
  LastNextAppointment result;

  for (const auto &event : events) {
    if (event.event_stat_id != 1 && event.event_stat_id != 2 && event.event_stat_id != 4) {
      continue; // only Scheduled/Completed/Confirmed count as "the appointment"
    }
    if (!event.start_date.has_value()) {
      continue;
    }

    if (*event.start_date < nowMs) {
      if (!result.last.has_value() || *event.start_date > *result.last->start_date) {
        result.last = event;
      }
    } else {
      if (!result.next.has_value() || *event.start_date < *result.next->start_date) {
        result.next = event;
      }
    }
  }

  return result;
}
```

- [ ] **Step 7: Run tests to verify they pass**

Run: `cmake --build build-release --target PsyClientManager_recurrence_tests && ./build-release/test/PsyClientManager_recurrence_tests`
Expected: PASS (all 5 new tests)

- [ ] **Step 8: Commit**

```bash
git add src/event_view/recurrence_utils.h src/event_view/recurrence_utils.cpp test/recurrence_utils_tests.cpp test/CMakeLists.txt
git commit -m "feat(recurrence): add eventsForClient and lastAndNextAppointment"
```

---

### Task 4: `ClientNotesPage` — merged feed, session entries, type filter, header summary

**Files:**
- Modify: `src/pages/client_notes_page/client_notes_page.h`
- Modify: `src/pages/client_notes_page/client_notes_page.cpp`
- Modify: `src/pages/client_notes_page/CMakeLists.txt`

**Interfaces:**
- Consumes: `pcm::recurrence::eventsForClient`, `pcm::recurrence::lastAndNextAppointment`, `pcm::recurrence::LastNextAppointment` (Task 3).
- Produces: no new public signals/slots; internal-only changes to `ClientNotesPage`.

- [ ] **Step 1: Add includes and new private members to the header**

In `src/pages/client_notes_page/client_notes_page.h`, add after the existing includes:

```cpp
#include "recurrence_utils.h"

#include <QVariant>
```

Add a `#include <variant>` next to the existing `<optional>` include.

Add a private enum, a private method declaration, and new member widgets. Replace the `private:` section's method list and member list with:

```cpp
private:
  enum class FeedFilter { All, Sessions, Notes };

  struct PendingAttachment {
    QString sourcePath;
    QString fileName;
    QString mimeType;
    bool isImage = false;
  };

  void buildUi();
  void reloadNotes();
  void clearNotes();
  void addNoteBubble(const DuckClientNote &note);
  void addSessionEntry(const DuckEvent &event);
  void addDateDivider(const QDate &date);
  void addAttachmentWidgets(QVBoxLayout *layout,
                            const std::vector<DuckClientNoteAttachment> &attachments);
  void refreshPendingAttachments();
  void updateAppointmentSummary(const QVector<DuckEvent> &events);
  [[nodiscard]] QString relativeNoteAttachmentPath(int64_t clientId,
                                                   int64_t noteId,
                                                   const QString &fileName) const;
  bool persistPendingAttachments(int64_t noteId);
  [[nodiscard]] QString currentClientTitle() const;

  std::shared_ptr<pcm::database::Database> mDb;
  std::optional<DuckClient> mCurrentClient;
  QList<PendingAttachment> mPendingAttachments;
  FeedFilter mFeedFilter = FeedFilter::All;

  QLabel *mClientNameLabel = nullptr;
  QLabel *mAppointmentSummaryLabel = nullptr;
  QPushButton *mOpenClientCardButton = nullptr;
  oclero::qlementine::SegmentedControl *mFeedFilterControl = nullptr;
  QScrollArea *mScrollArea = nullptr;
  QWidget *mFeedWidget = nullptr;
  QVBoxLayout *mFeedLayout = nullptr;
  QLabel *mEmptyLabel = nullptr;
  QPlainTextEdit *mComposer = nullptr;
  QLabel *mSaveStatusLabel = nullptr;
  QListWidget *mPendingAttachmentsList = nullptr;
  QPushButton *mAttachFilesButton = nullptr;
  QPushButton *mAddNoteButton = nullptr;
```

Add a forward declaration above the class so the header doesn't need the full qlementine header:

```cpp
namespace oclero::qlementine {
class SegmentedControl;
}
```

Add a new private slot declaration in the `private slots:` block:

```cpp
  void onFeedFilterChanged(int index);
```

- [ ] **Step 2: Build the filter control and summary label in `buildUi()`**

In `src/pages/client_notes_page/client_notes_page.cpp`, add includes at the top:

```cpp
#include <oclero/qlementine/widgets/SegmentedControl.hpp>

#include <algorithm>
#include <variant>
```

In `buildUi()`, directly after `mClientNameLabel` is constructed and before `mOpenClientCardButton` (around line 175-182), add:

```cpp
  mAppointmentSummaryLabel = new QLabel(headerSurface);
  mAppointmentSummaryLabel->setStyleSheet("color: rgba(255, 255, 255, 0.65);");
  mAppointmentSummaryLabel->setVisible(false);
```

Replace the header layout block (lines 187-190):

```cpp
  headerLayout->addWidget(mClientNameLabel);
  headerLayout->addStretch();
  headerLayout->addWidget(mOpenClientCardButton);
  rootLayout->addWidget(headerSurface);
```

with a version that stacks the breadcrumb+summary vertically on the left and adds the filter control before the "Open client card" button:

```cpp
  auto *titleColumn = new QVBoxLayout();
  titleColumn->setContentsMargins(0, 0, 0, 0);
  titleColumn->setSpacing(2);
  titleColumn->addWidget(mClientNameLabel);
  titleColumn->addWidget(mAppointmentSummaryLabel);

  mFeedFilterControl = new oclero::qlementine::SegmentedControl(headerSurface);
  mFeedFilterControl->addItem(tr("All"), {}, {}, QStringLiteral("all"));
  mFeedFilterControl->addItem(tr("Sessions"), {}, {}, QStringLiteral("sessions"));
  mFeedFilterControl->addItem(tr("Notes"), {}, {}, QStringLiteral("notes"));
  mFeedFilterControl->setCurrentIndex(0);

  headerLayout->addLayout(titleColumn);
  headerLayout->addStretch();
  headerLayout->addWidget(mFeedFilterControl);
  headerLayout->addWidget(mOpenClientCardButton);
  rootLayout->addWidget(headerSurface);
```

At the end of `buildUi()`, add the filter's signal connection alongside the existing `connect(...)` calls:

```cpp
  connect(mFeedFilterControl, &oclero::qlementine::SegmentedControl::currentIndexChanged, this,
          &ClientNotesPage::onFeedFilterChanged);
```

- [ ] **Step 3: Implement `onFeedFilterChanged`**

Add near the other `on*` slot implementations (after `onOpenClientCardClicked`):

```cpp
void ClientNotesPage::onFeedFilterChanged(const int index) {
  switch (index) {
  case 1:
    mFeedFilter = FeedFilter::Sessions;
    break;
  case 2:
    mFeedFilter = FeedFilter::Notes;
    break;
  default:
    mFeedFilter = FeedFilter::All;
    break;
  }
  reloadNotes();
}
```

- [ ] **Step 4: Merge notes and events in `reloadNotes()`**

Replace the body of `reloadNotes()` (lines 284-328) with a version that fetches both notes and events, always computes the appointment summary from the full unfiltered event set, then builds the visible feed according to `mFeedFilter`:

```cpp
void ClientNotesPage::reloadNotes() {
  clearNotes();

  if (!mCurrentClient.has_value() || mCurrentClient->id <= 0) {
    mEmptyLabel->setText(tr("Select a client to open notes."));
    mEmptyLabel->setVisible(true);
    mComposer->setEnabled(false);
    mAttachFilesButton->setEnabled(false);
    mAddNoteButton->setEnabled(false);
    mPendingAttachmentsList->setEnabled(false);
    mAppointmentSummaryLabel->setVisible(false);
    return;
  }

  mComposer->setEnabled(true);
  mAttachFilesButton->setEnabled(true);
  mAddNoteButton->setEnabled(true);
  mPendingAttachmentsList->setEnabled(true);

  const auto notes = mDb ? mDb->get_client_notes(mCurrentClient->id)
                         : std::vector<DuckClientNote>{};

  QVector<DuckEvent> events;
  if (mDb) {
    const auto windowStart = QDateTime::currentDateTime().addMonths(-3);
    const auto windowEnd = QDateTime::currentDateTime().addMonths(3);
    events = pcm::recurrence::eventsForClient(*mDb, mCurrentClient->id, windowStart, windowEnd);
  }
  updateAppointmentSummary(events);

  using FeedItem = std::variant<DuckClientNote, DuckEvent>;
  std::vector<FeedItem> items;
  if (mFeedFilter != FeedFilter::Sessions) {
    for (const auto &note : notes) {
      items.emplace_back(note);
    }
  }
  if (mFeedFilter != FeedFilter::Notes) {
    for (const auto &event : events) {
      items.emplace_back(event);
    }
  }

  if (items.empty()) {
    mEmptyLabel->setText(tr("No entries yet"));
    mEmptyLabel->setVisible(true);
    return;
  }

  const auto timestampOf = [](const FeedItem &item) -> qint64 {
    return std::visit(
        [](const auto &value) -> qint64 {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, DuckClientNote>) {
            return value.created_at.value_or(0);
          } else {
            return value.start_date.value_or(0);
          }
        },
        item);
  };
  std::sort(items.begin(), items.end(), [&](const FeedItem &left, const FeedItem &right) {
    return timestampOf(left) < timestampOf(right);
  });

  mEmptyLabel->setVisible(false);
  QDate previousDate;
  for (const auto &item : items) {
    const auto timestampMs = timestampOf(item);
    const auto itemDate = timestampMs > 0
                              ? QDateTime::fromMSecsSinceEpoch(timestampMs, QTimeZone::systemTimeZone()).date()
                              : QDate();
    if (itemDate.isValid() && itemDate != previousDate) {
      addDateDivider(itemDate);
      previousDate = itemDate;
    }
    std::visit(
        [this](const auto &value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, DuckClientNote>) {
            addNoteBubble(value);
          } else {
            addSessionEntry(value);
          }
        },
        item);
  }

  QMetaObject::invokeMethod(
      mScrollArea->verticalScrollBar(), "setValue", Qt::QueuedConnection,
      Q_ARG(int, mScrollArea->verticalScrollBar()->maximum()));
}
```

- [ ] **Step 5: Implement `updateAppointmentSummary`**

Add near `reloadNotes()`:

```cpp
void ClientNotesPage::updateAppointmentSummary(const QVector<DuckEvent> &events) {
  const auto nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
  const auto summary = pcm::recurrence::lastAndNextAppointment(events, nowMs);

  const auto describe = [](const std::optional<DuckEvent> &event) -> QString {
    if (!event.has_value() || !event->start_date.has_value()) {
      return {};
    }
    return QDateTime::fromMSecsSinceEpoch(*event->start_date, QTimeZone::systemTimeZone())
        .toString("dd.MM.yyyy HH:mm");
  };

  const auto lastText = describe(summary.last);
  const auto nextText = describe(summary.next);
  if (lastText.isEmpty() && nextText.isEmpty()) {
    mAppointmentSummaryLabel->setVisible(false);
    return;
  }

  QStringList parts;
  if (!lastText.isEmpty()) {
    parts << tr("Last: %1").arg(lastText);
  }
  if (!nextText.isEmpty()) {
    parts << tr("Next: %1").arg(nextText);
  }
  mAppointmentSummaryLabel->setText(parts.join(QStringLiteral("  ·  ")));
  mAppointmentSummaryLabel->setVisible(true);
}
```

- [ ] **Step 6: Implement `addSessionEntry`**

Add directly after `addNoteBubble`:

```cpp
void ClientNotesPage::addSessionEntry(const DuckEvent &event) {
  auto *card = new QFrame(mFeedWidget);
  card->setObjectName("sessionEntry");
  card->setMaximumWidth(pcm::widgets::constants::kNotesBubbleMaxWidth);
  card->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  card->setStyleSheet(
      "#sessionEntry {"
      " background: rgba(120, 170, 255, 0.08);"
      " border: 1px solid rgba(120, 170, 255, 0.18);"
      " border-radius: 12px;"
      "}");

  auto *layout = new QVBoxLayout(card);
  layout->setContentsMargins(
      pcm::widgets::constants::kNotesBubbleHorizontalPadding,
      pcm::widgets::constants::kNotesBubbleVerticalPadding,
      pcm::widgets::constants::kNotesBubbleHorizontalPadding,
      pcm::widgets::constants::kNotesBubbleVerticalPadding);
  layout->setSpacing(4);

  auto *timeLabel = new QLabel(card);
  const auto startAt =
      event.start_date.has_value()
          ? QDateTime::fromMSecsSinceEpoch(*event.start_date, QTimeZone::systemTimeZone())
          : QDateTime{};
  const auto title = QString::fromStdString(event.name.value_or(tr("Session").toStdString()));
  timeLabel->setText(QString("%1 · %2")
                         .arg(startAt.isValid() ? startAt.toString("dd.MM.yyyy HH:mm")
                                                : tr("Unknown time"),
                              title));
  timeLabel->setStyleSheet("color: rgba(255, 255, 255, 0.90); font-weight: 600;");

  auto *detailLabel = new QLabel(card);
  QString detailText;
  if (event.event_stat_id == 3 || event.event_stat_id == 6) {
    detailText = event.cancellation_reason.has_value()
                     ? QString::fromStdString(*event.cancellation_reason)
                     : tr("Canceled");
  } else if (event.cost.has_value()) {
    detailText = tr("Cost: %1").arg(QLocale().toCurrencyString(*event.cost));
  }
  detailLabel->setText(detailText);
  detailLabel->setStyleSheet("color: rgba(255, 255, 255, 0.55);");
  detailLabel->setVisible(!detailText.isEmpty());

  layout->addWidget(timeLabel);
  if (!detailText.isEmpty()) {
    layout->addWidget(detailLabel);
  } else {
    detailLabel->deleteLater();
  }

  mFeedLayout->insertWidget(mFeedLayout->count() - 1, card, 0, Qt::AlignLeft);
}
```

- [ ] **Step 7: Update `CMakeLists.txt`**

Replace `src/pages/client_notes_page/CMakeLists.txt` entirely:

```cmake
cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME ${PROJECT_NAME}_client_notes_page)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

qt_add_library(${TARGET_NAME} STATIC
        client_notes_page.cpp
        client_notes_page.h
)

target_include_directories(${TARGET_NAME} PRIVATE
        ${qlementine_SOURCE_DIR}/lib/include
)

add_dependencies(${TARGET_NAME}
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_widgets
        ${PROJECT_NAME}_event_view
)

target_link_libraries(${TARGET_NAME} PUBLIC
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_widgets
        ${PROJECT_NAME}_event_view
        qlementine
        Qt6::Widgets
)

target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
```

- [ ] **Step 8: Build**

Run: `cmake --build build-release --target PsyClientManager_client_notes_page`
Expected: builds with no errors.

- [ ] **Step 9: Commit**

```bash
git add src/pages/client_notes_page/client_notes_page.h src/pages/client_notes_page/client_notes_page.cpp src/pages/client_notes_page/CMakeLists.txt
git commit -m "feat(notes): interleave sessions into the client timeline feed"
```

---

### Task 5: `QClientInfoCardPage` — appointment summary widget

**Files:**
- Create: `src/pages/detail_client_info_page/appointment_summary_widget.h`
- Create: `src/pages/detail_client_info_page/appointment_summary_widget.cpp`
- Modify: `src/pages/detail_client_info_page/client_info_card.h`
- Modify: `src/pages/detail_client_info_page/client_info_card.cpp`
- Modify: `src/pages/detail_client_info_page/CMakeLists.txt`

**Interfaces:**
- Consumes: `pcm::recurrence::eventsForClient`, `pcm::recurrence::lastAndNextAppointment` (Task 3).
- Produces: `AppointmentSummaryWidget::setAppointments(const pcm::recurrence::LastNextAppointment &summary)`, `AppointmentSummaryWidget::clear()` — consumed only by `QClientInfoCardPage`.

- [ ] **Step 1: Create the widget header**

Create `src/pages/detail_client_info_page/appointment_summary_widget.h`:

```cpp
#pragma once

#include "recurrence_utils.h"

#include <QLabel>
#include <QWidget>

class AppointmentSummaryWidget final : public QWidget {
  Q_OBJECT

public:
  explicit AppointmentSummaryWidget(QWidget *parent = nullptr);

  void setAppointments(const pcm::recurrence::LastNextAppointment &summary);
  void clear();

private:
  QLabel *mLastLabel = nullptr;
  QLabel *mNextLabel = nullptr;
};
```

- [ ] **Step 2: Create the widget implementation**

Create `src/pages/detail_client_info_page/appointment_summary_widget.cpp`:

```cpp
#include "appointment_summary_widget.h"

#include <QDateTime>
#include <QTimeZone>
#include <QVBoxLayout>

AppointmentSummaryWidget::AppointmentSummaryWidget(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 8);
  layout->setSpacing(2);

  mLastLabel = new QLabel(this);
  mLastLabel->setStyleSheet("color: rgba(255, 255, 255, 0.65);");
  mNextLabel = new QLabel(this);
  mNextLabel->setStyleSheet("color: rgba(255, 255, 255, 0.65);");

  layout->addWidget(mLastLabel);
  layout->addWidget(mNextLabel);

  clear();
}

namespace {
QString describe(const std::optional<DuckEvent> &event) {
  if (!event.has_value() || !event->start_date.has_value()) {
    return {};
  }
  return QDateTime::fromMSecsSinceEpoch(*event->start_date, QTimeZone::systemTimeZone())
      .toString("dd.MM.yyyy HH:mm");
}
} // namespace

void AppointmentSummaryWidget::setAppointments(
    const pcm::recurrence::LastNextAppointment &summary) {
  const auto lastText = describe(summary.last);
  const auto nextText = describe(summary.next);

  mLastLabel->setText(lastText.isEmpty() ? tr("Last appointment: none")
                                         : tr("Last appointment: %1").arg(lastText));
  mNextLabel->setText(nextText.isEmpty() ? tr("Next appointment: none")
                                         : tr("Next appointment: %1").arg(nextText));
}

void AppointmentSummaryWidget::clear() {
  mLastLabel->setText(tr("Last appointment: none"));
  mNextLabel->setText(tr("Next appointment: none"));
}
```

- [ ] **Step 3: Wire the widget into `QClientInfoCardPage`**

In `src/pages/detail_client_info_page/client_info_card.h`, add the include:

```cpp
#include "appointment_summary_widget.h"
```

Add a new private member directly after `ClientChartsWidget *mChartsWidget = nullptr;`:

```cpp
  AppointmentSummaryWidget *mAppointmentSummaryWidget = nullptr;
```

Add a new private method declaration directly after `void refreshCharts() const;`:

```cpp
  void refreshAppointmentSummary() const;
```

- [ ] **Step 4: Construct and refresh the widget**

In `src/pages/detail_client_info_page/client_info_card.cpp`, add the include:

```cpp
#include "recurrence_utils.h"
```

In the constructor, directly after the `mChartsWidget` block (lines 23-26):

```cpp
  mAppointmentSummaryWidget = new AppointmentSummaryWidget(this);
  mUi->verticalLayout_3->insertWidget(
      mUi->verticalLayout_3->indexOf(mChartsWidget), mAppointmentSummaryWidget);
```

In `setClientInfo()`, directly after `refreshCharts();` (line 49):

```cpp
  refreshAppointmentSummary();
```

Add the implementation directly after `refreshCharts()` (~line 247):

```cpp
void QClientInfoCardPage::refreshAppointmentSummary() const {
  if (!mAppointmentSummaryWidget) {
    return;
  }
  if (!mDb || mClientInfo.getId() <= 0) {
    mAppointmentSummaryWidget->clear();
    return;
  }

  const auto windowStart = QDateTime::currentDateTime().addMonths(-3);
  const auto windowEnd = QDateTime::currentDateTime().addMonths(3);
  const auto events =
      pcm::recurrence::eventsForClient(*mDb, mClientInfo.getId(), windowStart, windowEnd);
  const auto nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
  mAppointmentSummaryWidget->setAppointments(
      pcm::recurrence::lastAndNextAppointment(events, nowMs));
}
```

- [ ] **Step 5: Update `CMakeLists.txt`**

In `src/pages/detail_client_info_page/CMakeLists.txt`, add `appointment_summary_widget.cpp` to the `qt_add_library` sources:

```cmake
qt_add_library(${TARGET_NAME} STATIC
        client_info_card.cpp
        client_charts_widget.cpp
        appointment_summary_widget.cpp
        qclient.cpp
        qclient.h
)
```

Add `${PROJECT_NAME}_event_view` to both `add_dependencies` and `target_link_libraries`:

```cmake
add_dependencies(${TARGET_NAME}
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_event_view
)
```

```cmake
target_link_libraries(${TARGET_NAME} PUBLIC
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_event_view
        qlementine
        qcustomplot
        Qt6::Widgets
        Qt6::PrintSupport
)
```

- [ ] **Step 6: Build**

Run: `cmake --build build-release --target PsyClientManager_client_info_card_page`
Expected: builds with no errors.

- [ ] **Step 7: Commit**

```bash
git add src/pages/detail_client_info_page/appointment_summary_widget.h src/pages/detail_client_info_page/appointment_summary_widget.cpp src/pages/detail_client_info_page/client_info_card.h src/pages/detail_client_info_page/client_info_card.cpp src/pages/detail_client_info_page/CMakeLists.txt
git commit -m "feat(client-card): show last/next appointment summary"
```

---

### Task 6: Full build, manual smoke test, version bump, CHANGELOG

**Files:**
- Modify: `CMakeLists.txt` (version)
- Modify: `src/app/application.cpp` (version string)
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Full build**

Run: `cmake --build build-release`
Expected: builds with no errors, all existing targets still compile (no fallout in `event_info_page`, `timeline_widget`, etc. from the `recurrence_utils.h` changes).

- [ ] **Step 2: Run the full test suite**

Run: `ctest --test-dir build-release --output-on-failure`
Expected: all tests pass, including the new `PsyClientManager_database_tests` and `PsyClientManager_recurrence_tests` cases from Tasks 1-3.

- [ ] **Step 3: Manual smoke test**

Launch the built binary directly (not via `timeout &`, so `$!` is the real PID):

```bash
./build-release/src/app/PsyClientManager &
APP_PID=$!
sleep 5
kill -0 $APP_PID && echo "still running" || echo "crashed"
kill $APP_PID
```

Expected: "still running" (no SIGSEGV). Then interactively open a client's Notes screen, confirm session entries render alongside notes, the All/Sessions/Notes filter changes what's shown, and the header/client-card summaries show sensible last/next appointment text (or "none" for a client with no events).

- [ ] **Step 4: Bump version**

In `CMakeLists.txt` line 11, change:

```cmake
project(PsyClientManager VERSION 0.1.17 LANGUAGES CXX)
```

to:

```cmake
project(PsyClientManager VERSION 0.1.18 LANGUAGES CXX)
```

In `src/app/application.cpp` line 33, change:

```cpp
  app.setApplicationVersion("0.1.17");
```

to:

```cpp
  app.setApplicationVersion("0.1.18");
```

- [ ] **Step 5: Add CHANGELOG entry**

In `CHANGELOG.md`, add a new section directly above the existing `## [0.1.17]` entry:

```markdown
## [0.1.18] - 2026-07-30

### Added

- Unified client timeline: the Notes screen now interleaves session
  entries with notes in one chronological feed, with an All/Sessions/Notes
  filter and a last/next-appointment summary. The same summary now also
  appears on the client card.
```

- [ ] **Step 6: Rebuild to confirm the version bump compiles**

Run: `cmake --build build-release --target PsyClientManager`
Expected: builds with no errors.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt src/app/application.cpp CHANGELOG.md
git commit -m "chore: bump version to 0.1.18"
```

---

## Self-Review

**Spec coverage:**
- Feed lives on existing Notes screen, no new page/nav — Task 4. ✓
- Virtual occurrences shown in bounded ±3-month window, materialized events unbounded — Task 3 (`eventsForClient`), Task 4/5 callers pass `addMonths(-3)`/`addMonths(3)`. ✓
- Last/next appointment in both header and client card — Task 4 (`updateAppointmentSummary`) and Task 5 (`AppointmentSummaryWidget`). ✓
- Type filter (All/Sessions/Notes) via `SegmentedControl`, not `QComboBox` — Task 4 Step 2. ✓
- Out-of-scope items (click-to-open-event, system-log lines, in-place editing, cross-client search) — not implemented anywhere in this plan. ✓
- DB round-trip tests for both new `Database` methods — Task 1 Step 1, Task 2 Step 1. ✓
- DB-backed test for `eventsForClient` covering combined result + no-duplicate-for-materialized-occurrence — Task 3 Step 1 (two tests). ✓
- Pure unit tests for `lastAndNextAppointment` (past-only, future-only, canceled-skipped) — Task 3 Step 1 (three tests). ✓
- No dedicated UI test for `ClientNotesPage`/`QClientInfoCardPage` — confirmed, only a manual smoke test in Task 6. ✓

**Placeholder scan:** No TBD/TODO markers; every step has concrete code.

**Type consistency:** `QVector<DuckEvent> eventsForClient(...)` (Task 3) matches its use in Task 4 (`events` local var) and Task 5 (`refreshAppointmentSummary`). `LastNextAppointment` struct fields (`last`, `next`) match usage in `updateAppointmentSummary` and `AppointmentSummaryWidget::describe`. `FeedFilter` enum values (`All`/`Sessions`/`Notes`) match the `SegmentedControl` index mapping in `onFeedFilterChanged`.

---

**Plan complete and saved to `docs/superpowers/plans/2026-07-30-client-timeline.md`. Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
