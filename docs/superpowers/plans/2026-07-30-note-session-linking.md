# Note-Session Linking, Event Navigation, and Jump-to-Latest Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a note be linked to the session it's about (surviving materialization of virtual recurring occurrences), let clicking a session entry or a note's session badge navigate to that event in the Calendar, and add a jump-to-latest control to the Notes feed.

**Architecture:** `ClientNote` gains three nullable link columns (`linked_event_id`, or `linked_series_id`+`linked_occurrence_start`). A new `pcm::recurrence::resolveNoteLink` resolves either form back to a `DuckEvent` — materialized directly, or rebuilt as a virtual occurrence, or "graduated" to the now-materialized row if one has since appeared. `ClientNotesPage` gains a composer link picker and a note-bubble badge, both driving a new `openEventRequested(eventId, dayMs)` signal that `MainWindow` routes through `QEventInfoPage::openEventOnDay` to select the day and open the event.

**Tech Stack:** C++20, Qt6 Widgets, DuckDB (C++ client), GoogleTest.

## Global Constraints

- Schema change (three new `ClientNote` columns) requires a migration (`ALTER TABLE ... ADD COLUMN IF NOT EXISTS`) and a DB round-trip test, per AGENTS.md.
- Every MR needs an issue (#57), a branch (`feat/57-client-journal-followups`, already created off `feat/30-client-timeline`), a version bump, and a CHANGELOG entry (final task).
- No `QComboBox` in widgets constructed before `mMainWindow->show()` — the composer's link picker uses a `QMenu`, not a `QComboBox` or plain popup `QListWidget` that could hit the same construction-timing hazard.
- Design doc: `docs/superpowers/specs/2026-07-30-note-session-linking-design.md`.

---

### Task 1: `ClientNote` link columns — schema, migration, insert/select

**Files:**
- Modify: `src/database/schema.hpp` (`DuckClientNote` struct + constructor + `operator<<`)
- Modify: `src/database/constants.hpp` (migration block, `kInsertClientNoteQuery`, `kSelectClientNotesQuery`)
- Modify: `src/database/database.cpp` (`Database::add_client_note`)
- Test: `test/database_tests.cpp`

**Interfaces:**
- Produces: `DuckClientNote::linked_event_id`, `linked_series_id`, `linked_occurrence_start_ms` (all `std::optional<std::int64_t>`), and `Database::add_client_note` now persists them when set on the passed-in `DuckClientNote`.

- [ ] **Step 1: Write the failing test**

Append to `test/database_tests.cpp` (after `GetEventSeriesForClientAndRangeFiltersByClientAndRange`):

```cpp
TEST(DatabaseTest, ClientNoteRoundTripsLinkedEventId) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_note_linked_event")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  DuckClient client;
  client.name = std::string{"Gina"};
  client.last_name = std::string{"G"};
  const auto clientId = db.add_client(client);
  ASSERT_GT(clientId, 0);

  DuckEvent event;
  event.name = std::string{"Session"};
  event.start_date = 1730000000000;
  event.end_date = 1730003600000;
  const auto eventId = db.add_event(event);
  ASSERT_GT(eventId, 0);

  DuckClientNote note;
  note.client_id = clientId;
  note.body_markdown = std::string{"Linked to a real session"};
  note.linked_event_id = eventId;
  const auto noteId = db.add_client_note(note);
  ASSERT_GT(noteId, 0);

  const auto notes = db.get_client_notes(clientId);
  ASSERT_EQ(notes.size(), 1);
  ASSERT_TRUE(notes.front().linked_event_id.has_value());
  EXPECT_EQ(*notes.front().linked_event_id, eventId);
  EXPECT_FALSE(notes.front().linked_series_id.has_value());
  EXPECT_FALSE(notes.front().linked_occurrence_start_ms.has_value());

  db_dir.remove(true);
}

TEST(DatabaseTest, ClientNoteRoundTripsLinkedSeriesOccurrence) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_note_linked_series")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  DuckClient client;
  client.name = std::string{"Hank"};
  client.last_name = std::string{"H"};
  const auto clientId = db.add_client(client);
  ASSERT_GT(clientId, 0);

  DuckEventSeries series;
  series.name = std::string{"Weekly"};
  series.client_id = clientId;
  series.start_date = 1730000000000;
  series.end_date = 1730003600000;
  series.duration = 3600;
  series.recurrence_rule = "FREQ=WEEKLY;INTERVAL=1";
  const auto seriesId = db.add_event_series(series);
  ASSERT_GT(seriesId, 0);

  DuckClientNote note;
  note.client_id = clientId;
  note.body_markdown = std::string{"Linked to a virtual occurrence"};
  note.linked_series_id = seriesId;
  note.linked_occurrence_start_ms = 1730600000000;
  const auto noteId = db.add_client_note(note);
  ASSERT_GT(noteId, 0);

  const auto notes = db.get_client_notes(clientId);
  ASSERT_EQ(notes.size(), 1);
  EXPECT_FALSE(notes.front().linked_event_id.has_value());
  ASSERT_TRUE(notes.front().linked_series_id.has_value());
  EXPECT_EQ(*notes.front().linked_series_id, seriesId);
  ASSERT_TRUE(notes.front().linked_occurrence_start_ms.has_value());
  EXPECT_EQ(*notes.front().linked_occurrence_start_ms, 1730600000000);

  db_dir.remove(true);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-release --target PsyClientManager_database_tests && ./build-release/test/PsyClientManager_database_tests --gtest_filter='*ClientNoteRoundTrips*'`
Expected: FAIL to compile — `DuckClientNote` has no `linked_event_id` member.

- [ ] **Step 3: Extend `DuckClientNote`**

In `src/database/schema.hpp`, replace the `DuckClientNote` struct and its `operator<<`:

```cpp
struct DuckClientNote {
  std::int64_t id = -1;
  std::int64_t client_id = -1;
  std::optional<std::string> body_markdown = std::nullopt;
  std::optional<std::int64_t> created_at = std::nullopt;
  std::optional<std::int64_t> updated_at = std::nullopt;
  std::optional<std::int64_t> linked_event_id = std::nullopt;
  std::optional<std::int64_t> linked_series_id = std::nullopt;
  std::optional<std::int64_t> linked_occurrence_start_ms = std::nullopt;

  DuckClientNote() = default;
  DuckClientNote(const duckdb::DataChunk &chunk, duckdb::idx_t index) {
    id = db_utils::toInt32AsInt64(chunk.GetValue(0, index));
    client_id = db_utils::toInt32AsInt64(chunk.GetValue(1, index));
    body_markdown = db_utils::toOptionalString(chunk.GetValue(2, index));
    created_at = db_utils::toOptionalTimestampMs(chunk.GetValue(3, index));
    updated_at = db_utils::toOptionalTimestampMs(chunk.GetValue(4, index));
    if (chunk.ColumnCount() > 5) {
      linked_event_id = db_utils::toOptionalInt32AsInt64(chunk.GetValue(5, index));
    }
    if (chunk.ColumnCount() > 6) {
      linked_series_id = db_utils::toOptionalInt32AsInt64(chunk.GetValue(6, index));
    }
    if (chunk.ColumnCount() > 7) {
      linked_occurrence_start_ms =
          db_utils::toOptionalTimestampMs(chunk.GetValue(7, index));
    }
  }
};
inline std::ostream &operator<<(std::ostream &os, const DuckClientNote &note) {
  os << "DuckClientNote{id=" << note.id << ", client_id=" << note.client_id
     << ", body_markdown=";
  print_optional(os, note.body_markdown) << ", created_at=";
  print_optional(os, note.created_at) << ", updated_at=";
  print_optional(os, note.updated_at) << ", linked_event_id=";
  print_optional(os, note.linked_event_id) << ", linked_series_id=";
  print_optional(os, note.linked_series_id) << ", linked_occurrence_start_ms=";
  print_optional(os, note.linked_occurrence_start_ms) << "}";
  return os;
}
```

- [ ] **Step 4: Add the migration**

In `src/database/constants.hpp`, at the end of the `kSchemaMigrations` block (directly after the `EventSeriesException` `ALTER TABLE` lines, before the closing `)duckdb";`):

```sql
ALTER TABLE ClientNote ADD COLUMN IF NOT EXISTS linked_event_id INTEGER;
ALTER TABLE ClientNote ADD COLUMN IF NOT EXISTS linked_series_id INTEGER;
ALTER TABLE ClientNote ADD COLUMN IF NOT EXISTS linked_occurrence_start TIMESTAMP;
```

- [ ] **Step 5: Update the insert and select queries**

In `src/database/constants.hpp`, replace `kInsertClientNoteQuery`:

```cpp
constexpr auto kInsertClientNoteQuery = R"duckdb(
INSERT INTO ClientNote (
    id, client_id, body_markdown, created_at, updated_at,
    linked_event_id, linked_series_id, linked_occurrence_start
)
SELECT COALESCE(MAX(id), 0) + 1, $1, $2, $3, $4, $5, $6, $7
FROM ClientNote
RETURNING id
)duckdb";
```

Replace `kSelectClientNotesQuery`:

```cpp
constexpr auto kSelectClientNotesQuery = R"duckdb(
SELECT id, client_id, body_markdown, created_at, updated_at,
       linked_event_id, linked_series_id, linked_occurrence_start
FROM ClientNote
WHERE client_id = $1
ORDER BY created_at ASC, id ASC
)duckdb";
```

- [ ] **Step 6: Bind the new parameters in `add_client_note`**

In `src/database/database.cpp`, replace the parameter vector in `Database::add_client_note`:

```cpp
  duckdb::Connection conn(*mDb);
  auto result = executePrepared(
      conn, constance::kInsertClientNoteQuery,
      duckdb::vector<duckdb::Value>{
          duckdb::Value::BIGINT(note.client_id),
          db_utils::toDuckValue(note.body_markdown),
          db_utils::toDuckTimestamp(createdAtMs * 1000),
          db_utils::toDuckTimestamp(updatedAtMs * 1000),
          db_utils::toDuckValue(note.linked_event_id),
          db_utils::toDuckValue(note.linked_series_id),
          note.linked_occurrence_start_ms.has_value()
              ? db_utils::toDuckTimestamp(std::make_optional(*note.linked_occurrence_start_ms * 1000))
              : db_utils::toDuckTimestamp(std::nullopt)});
```

- [ ] **Step 7: Run tests to verify they pass**

Run: `cmake --build build-release --target PsyClientManager_database_tests && ./build-release/test/PsyClientManager_database_tests --gtest_filter='*ClientNoteRoundTrips*'`
Expected: PASS (both new tests)

- [ ] **Step 8: Commit**

```bash
git add src/database/schema.hpp src/database/constants.hpp src/database/database.cpp test/database_tests.cpp
git commit -m "feat(database): add note-session link columns to ClientNote"
```

---

### Task 2: `Database::get_event_by_series_occurrence`

**Files:**
- Modify: `src/database/constants.hpp` (query constant near `kSelectMaterializedOccurrenceStartsForSeriesQuery`)
- Modify: `src/database/database.h` (declaration near `get_materialized_occurrence_starts_for_series`)
- Modify: `src/database/database.cpp` (definition)
- Test: `test/database_tests.cpp`

**Interfaces:**
- Produces: `std::unique_ptr<DuckEvent> Database::get_event_by_series_occurrence(int64_t series_id, int64_t occurrence_start_ms)` — the materialized `Event` row for that series+occurrence, or `nullptr` if not yet materialized. Consumed by Task 3's `resolveNoteLink`.

- [ ] **Step 1: Write the failing test**

Append to `test/database_tests.cpp`:

```cpp
TEST(DatabaseTest, GetEventBySeriesOccurrenceFindsMaterializedRow) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_event_by_series_occurrence")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  DuckEventSeries series;
  series.name = std::string{"Weekly"};
  series.start_date = 1730000000000;
  series.end_date = 1730003600000;
  series.duration = 3600;
  series.recurrence_rule = "FREQ=WEEKLY;INTERVAL=1";
  const auto seriesId = db.add_event_series(series);
  ASSERT_GT(seriesId, 0);

  DuckEvent materialized;
  materialized.name = std::string{"Weekly (materialized)"};
  materialized.start_date = 1730600000000;
  materialized.end_date = 1730603600000;
  materialized.series_id = seriesId;
  materialized.original_occurrence_start = 1730600000000;
  const auto materializedId = db.add_event(materialized);
  ASSERT_GT(materializedId, 0);

  const auto found = db.get_event_by_series_occurrence(seriesId, 1730600000000);
  ASSERT_NE(found, nullptr);
  EXPECT_EQ(found->id, materializedId);

  const auto notFound = db.get_event_by_series_occurrence(seriesId, 1731200000000);
  EXPECT_EQ(notFound, nullptr);

  db_dir.remove(true);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-release --target PsyClientManager_database_tests && ./build-release/test/PsyClientManager_database_tests --gtest_filter='*GetEventBySeriesOccurrence*'`
Expected: FAIL to compile — `get_event_by_series_occurrence` is not a member of `Database`.

- [ ] **Step 3: Add the query constant**

In `src/database/constants.hpp`, directly after `kSelectMaterializedOccurrenceStartsForSeriesQuery`:

```cpp
constexpr auto kSelectEventBySeriesOccurrenceQuery = R"duckdb(
SELECT * FROM Event
WHERE series_id = $1 AND original_occurrence_start = $2
LIMIT 1
)duckdb";
```

- [ ] **Step 4: Declare the method**

In `src/database/database.h`, directly after `get_materialized_occurrence_starts_for_series`:

```cpp
  std::unique_ptr<DuckEvent> get_event_by_series_occurrence(int64_t series_id,
                                                             int64_t occurrence_start_ms);
```

- [ ] **Step 5: Implement the method**

In `src/database/database.cpp`, directly after `Database::get_materialized_occurrence_starts_for_series`, mirroring `Database::get_event`'s structure:

```cpp
std::unique_ptr<DuckEvent> Database::get_event_by_series_occurrence(
    const int64_t series_id, const int64_t occurrence_start_ms) {
  if (series_id <= 0) {
    return nullptr;
  }

  duckdb::Connection conn(*mDb);
  auto result = executePrepared(
      conn, constance::kSelectEventBySeriesOccurrenceQuery,
      {duckdb::Value::BIGINT(series_id),
       db_utils::toDuckTimestamp(std::make_optional(occurrence_start_ms * 1000))});
  if (!result || result->HasError()) {
    PLOG_ERROR << "Failed to query event by series occurrence (series_id=" << series_id
               << "): " << (result ? result->GetError() : "prepare failed");
    return nullptr;
  }

  auto chunk = result->Fetch();
  if (!chunk || chunk->size() == 0) {
    return nullptr;
  }

  return std::make_unique<DuckEvent>(*chunk, 0);
}
```

- [ ] **Step 6: Run test to verify it passes**

Run: `cmake --build build-release --target PsyClientManager_database_tests && ./build-release/test/PsyClientManager_database_tests --gtest_filter='*GetEventBySeriesOccurrence*'`
Expected: PASS

- [ ] **Step 7: Commit**

```bash
git add src/database/constants.hpp src/database/database.h src/database/database.cpp test/database_tests.cpp
git commit -m "feat(database): add get_event_by_series_occurrence"
```

---

### Task 3: `pcm::recurrence::resolveNoteLink`

**Files:**
- Modify: `src/event_view/recurrence_utils.h`
- Modify: `src/event_view/recurrence_utils.cpp`
- Test: `test/recurrence_utils_tests.cpp`

**Interfaces:**
- Consumes: `Database::get_event`, `Database::get_event_by_series_occurrence` (Task 2), `Database::get_event_series` (existing), `pcm::recurrence::occurrences`, `pcm::recurrence::buildVirtualOccurrence` (existing).
- Produces: `std::optional<DuckEvent> resolveNoteLink(pcm::database::Database &db, const DuckClientNote &note)`. Consumed by Task 5 (`ClientNotesPage::addNoteBubble`).

- [ ] **Step 1: Write the failing tests**

Append to `test/recurrence_utils_tests.cpp`:

```cpp
TEST(RecurrenceUtilsTest, ResolveNoteLinkFindsMaterializedEventById) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_resolve_link_by_id")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  DuckEvent event;
  event.name = std::string{"Session"};
  event.start_date = 1730000000000;
  event.end_date = 1730003600000;
  const auto eventId = db.add_event(event);
  ASSERT_GT(eventId, 0);

  DuckClientNote note;
  note.linked_event_id = eventId;

  const auto resolved = pcm::recurrence::resolveNoteLink(db, note);
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved->id, eventId);

  db_dir.remove(true);
}

TEST(RecurrenceUtilsTest, ResolveNoteLinkRebuildsUnmaterializedVirtualOccurrence) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_resolve_link_virtual")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  const auto seriesStartLocal = QDateTime(QDate::currentDate().addDays(1), QTime(10, 0, 0));
  DuckEventSeries series;
  series.name = std::string{"Weekly"};
  series.start_date = seriesStartLocal.toUTC().toMSecsSinceEpoch();
  series.end_date = *series.start_date + 3'600'000;
  series.duration = 3600;
  series.recurrence_rule = pcm::recurrence::weeklyRuleForDate(seriesStartLocal.date()).toStdString();
  const auto seriesId = db.add_event_series(series);
  ASSERT_GT(seriesId, 0);

  DuckClientNote note;
  note.linked_series_id = seriesId;
  note.linked_occurrence_start_ms = *series.start_date;

  const auto resolved = pcm::recurrence::resolveNoteLink(db, note);
  ASSERT_TRUE(resolved.has_value());
  EXPECT_TRUE(resolved->is_virtual_occurrence);
  EXPECT_EQ(resolved->series_id, seriesId);
  EXPECT_EQ(*resolved->start_date, *series.start_date);

  db_dir.remove(true);
}

TEST(RecurrenceUtilsTest, ResolveNoteLinkFindsMaterializedRowForSinceMaterializedOccurrence) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_resolve_link_graduated")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  const auto seriesStartLocal = QDateTime(QDate::currentDate().addDays(1), QTime(10, 0, 0));
  DuckEventSeries series;
  series.name = std::string{"Weekly"};
  series.start_date = seriesStartLocal.toUTC().toMSecsSinceEpoch();
  series.end_date = *series.start_date + 3'600'000;
  series.duration = 3600;
  series.recurrence_rule = pcm::recurrence::weeklyRuleForDate(seriesStartLocal.date()).toStdString();
  const auto seriesId = db.add_event_series(series);
  ASSERT_GT(seriesId, 0);

  DuckEvent materialized;
  materialized.name = std::string{"Weekly (materialized)"};
  materialized.start_date = *series.start_date;
  materialized.end_date = *materialized.start_date + 3'600'000;
  materialized.series_id = seriesId;
  materialized.original_occurrence_start = materialized.start_date;
  const auto materializedId = db.add_event(materialized);
  ASSERT_GT(materializedId, 0);

  DuckClientNote note;
  note.linked_series_id = seriesId;
  note.linked_occurrence_start_ms = *series.start_date;

  const auto resolved = pcm::recurrence::resolveNoteLink(db, note);
  ASSERT_TRUE(resolved.has_value());
  EXPECT_FALSE(resolved->is_virtual_occurrence);
  EXPECT_EQ(resolved->id, materializedId);

  db_dir.remove(true);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build-release --target PsyClientManager_recurrence_tests`
Expected: FAIL to compile — `resolveNoteLink` is not declared in `pcm::recurrence`.

- [ ] **Step 3: Declare the function**

In `src/event_view/recurrence_utils.h`, append inside `namespace pcm::recurrence` after `lastAndNextAppointment`:

```cpp
std::optional<DuckEvent> resolveNoteLink(pcm::database::Database &db, const DuckClientNote &note);
```

- [ ] **Step 4: Implement the function**

In `src/event_view/recurrence_utils.cpp`, append inside `namespace pcm::recurrence` after `lastAndNextAppointment`:

```cpp
std::optional<DuckEvent> resolveNoteLink(pcm::database::Database &db, const DuckClientNote &note) {
  if (note.linked_event_id.has_value()) {
    auto event = db.get_event(*note.linked_event_id);
    if (!event) {
      return std::nullopt;
    }
    return *event;
  }

  if (!note.linked_series_id.has_value() || !note.linked_occurrence_start_ms.has_value()) {
    return std::nullopt;
  }

  auto materialized =
      db.get_event_by_series_occurrence(*note.linked_series_id, *note.linked_occurrence_start_ms);
  if (materialized) {
    return *materialized;
  }

  auto series = db.get_event_series(*note.linked_series_id);
  if (!series) {
    return std::nullopt;
  }

  const auto occurrenceStart =
      QDateTime::fromMSecsSinceEpoch(*note.linked_occurrence_start_ms, QTimeZone::UTC)
          .toTimeZone(QTimeZone::systemTimeZone());
  const auto virtualId =
      -(series->id * 1'000'000LL + static_cast<int64_t>(occurrenceStart.date().toJulianDay()));
  return buildVirtualOccurrence(*series, occurrenceStart, virtualId);
}
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build-release --target PsyClientManager_recurrence_tests && ./build-release/test/PsyClientManager_recurrence_tests`
Expected: PASS (all tests, including the 3 new ones)

- [ ] **Step 6: Commit**

```bash
git add src/event_view/recurrence_utils.h src/event_view/recurrence_utils.cpp test/recurrence_utils_tests.cpp
git commit -m "feat(recurrence): add resolveNoteLink"
```

---

### Task 4: Backup/restore round-trip for linked notes

**Files:**
- Test: `test/backup_tests.cpp`

**Interfaces:**
- Consumes: `Database::add_client_note`, `Database::get_client_notes` (Task 1), `pcm::backup::BackupService`, `pcm::backup::RestoreService` (existing).

No production code changes — `export_snapshot`/restore use DuckDB's `EXPORT DATABASE ... FORMAT PARQUET`, which carries all columns automatically.

- [ ] **Step 1: Write the test**

Append to `test/backup_tests.cpp`, directly after `RestoresSeriesOccurrenceReminderState`:

```cpp
TEST(RestoreServiceTest, RestoresLinkedNoteFields) {
  auto sourceDb = makeTestDatabase("tmp_restore_note_link_source");

  DuckClient client;
  client.name = std::string{"Ivy"};
  client.last_name = std::string{"I"};
  const auto clientId = sourceDb.add_client(client);
  ASSERT_GT(clientId, 0);

  DuckEvent event;
  event.name = std::string{"Session"};
  event.start_date = 1730000000000;
  event.end_date = 1730003600000;
  const auto eventId = sourceDb.add_event(event);
  ASSERT_GT(eventId, 0);

  DuckClientNote note;
  note.client_id = clientId;
  note.body_markdown = std::string{"Linked note"};
  note.linked_event_id = eventId;
  const auto noteId = sourceDb.add_client_note(note);
  ASSERT_GT(noteId, 0);

  const auto backupPath = Poco::Path(Poco::Path::current())
                              .append("tmp_restore_note_link.psybackup")
                              .toString();
  if (Poco::File(backupPath).exists()) {
    Poco::File(backupPath).remove();
  }
  ASSERT_TRUE(pcm::backup::BackupService{}.create_backup(sourceDb, backupPath).ok);

  const auto targetPath = Poco::Path(Poco::Path::current())
                              .append("tmp_restore_note_link_target")
                              .toString();
  if (Poco::File(targetPath).exists()) {
    Poco::File(targetPath).remove(true);
  }

  const auto restoreResult =
      pcm::backup::RestoreService{}.restore_backup(backupPath, targetPath);
  ASSERT_TRUE(restoreResult.ok) << restoreResult.error;

  pcm::config::Config targetConfig{
      .db_conf = pcm::config::DatabaseConfig{.db_pth = Poco::Path(targetPath)}};
  pcm::database::Database restoredDb{targetConfig};
  const auto notes = restoredDb.get_client_notes(clientId);
  ASSERT_EQ(notes.size(), 1);
  ASSERT_TRUE(notes.front().linked_event_id.has_value());
  EXPECT_EQ(*notes.front().linked_event_id, eventId);

  Poco::File(backupPath).remove();
  Poco::File(targetPath).remove(true);
  Poco::File(Poco::Path(Poco::Path::current()).append("tmp_restore_note_link_source"))
      .remove(true);
}
```

- [ ] **Step 2: Run the test**

Run: `cmake --build build-release --target PsyClientManager_backup_tests && ./build-release/test/PsyClientManager_backup_tests --gtest_filter='*RestoresLinkedNoteFields*'`
Expected: PASS (no production code changes needed — this confirms `EXPORT DATABASE`/restore already carries the new columns)

- [ ] **Step 3: Commit**

```bash
git add test/backup_tests.cpp
git commit -m "test(backup): verify linked-note fields survive backup/restore"
```

---

### Task 5: `ClientNotesPage` — link picker, note badge, `openEventRequested`

**Files:**
- Modify: `src/pages/client_notes_page/client_notes_page.h`
- Modify: `src/pages/client_notes_page/client_notes_page.cpp`

**Interfaces:**
- Consumes: `pcm::recurrence::resolveNoteLink` (Task 3).
- Produces: `signal void ClientNotesPage::openEventRequested(int64_t eventId, qint64 dayMs)`, consumed by Task 7 (`MainWindow`).

- [ ] **Step 1: Add new members and signal to the header**

In `src/pages/client_notes_page/client_notes_page.h`, add to `signals:`:

```cpp
  void openEventRequested(int64_t eventId, qint64 dayMs);
```

Add new private methods (in the existing `private:` method list, after `updateAppointmentSummary`):

```cpp
  void onLinkSessionButtonClicked();
  void updateLinkButtonText();
  [[nodiscard]] std::optional<DuckEvent> nearestPastEvent(const QVector<DuckEvent> &events) const;
```

Add new private members (after `mFeedFilter`):

```cpp
  QVector<DuckEvent> mCachedFeedEvents;
  std::optional<DuckEvent> mPendingLinkedEvent;
  bool mLinkManuallySet = false;
```

Add a new widget member (after `mAddNoteButton`):

```cpp
  QPushButton *mLinkSessionButton = nullptr;
```

- [ ] **Step 2: Build the link button in `buildUi()`**

In `src/pages/client_notes_page/client_notes_page.cpp`, in `buildUi()`, directly after `mAttachFilesButton` is constructed:

```cpp
  mLinkSessionButton = new QPushButton(tr("Link to a session"), composerSurface);
  mLinkSessionButton->setCursor(Qt::PointingHandCursor);
```

In the `actionsLayout` block, add it between `mAttachFilesButton` and the stretch:

```cpp
  actionsLayout->addWidget(mAttachFilesButton, 0);
  actionsLayout->addWidget(mLinkSessionButton, 0);
  actionsLayout->addStretch();
  actionsLayout->addWidget(mAddNoteButton, 0);
```//

(Replace the existing three-line block that only had `mAttachFilesButton`/stretch/`mAddNoteButton` with this four-line version.)

Add its connection alongside the others at the end of `buildUi()`:

```cpp
  connect(mLinkSessionButton, &QPushButton::clicked, this,
          &ClientNotesPage::onLinkSessionButtonClicked);
```

- [ ] **Step 3: Cache feed events and compute the default link in `reloadNotes()`**

In `reloadNotes()`, directly after the block that computes `events` (the `pcm::recurrence::eventsForClient` call) and calls `updateAppointmentSummary(events)`, add:

```cpp
  mCachedFeedEvents = events;
  if (!mLinkManuallySet) {
    mPendingLinkedEvent = nearestPastEvent(events);
  }
  updateLinkButtonText();
```

- [ ] **Step 4: Implement `nearestPastEvent`, `onLinkSessionButtonClicked`, `updateLinkButtonText`**

Add near `updateAppointmentSummary`:

```cpp
std::optional<DuckEvent> ClientNotesPage::nearestPastEvent(const QVector<DuckEvent> &events) const {
  const auto nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
  std::optional<DuckEvent> best;
  for (const auto &event : events) {
    if (!event.start_date.has_value() || *event.start_date > nowMs) {
      continue;
    }
    if (!best.has_value() || *event.start_date > *best->start_date) {
      best = event;
    }
  }
  return best;
}

void ClientNotesPage::updateLinkButtonText() {
  if (!mLinkSessionButton) {
    return;
  }
  if (!mPendingLinkedEvent.has_value() || !mPendingLinkedEvent->start_date.has_value()) {
    mLinkSessionButton->setText(tr("Link to a session"));
    return;
  }
  const auto startAt = QDateTime::fromMSecsSinceEpoch(*mPendingLinkedEvent->start_date,
                                                       QTimeZone::systemTimeZone());
  mLinkSessionButton->setText(tr("Linked: %1").arg(startAt.toString("dd.MM HH:mm")));
}

void ClientNotesPage::onLinkSessionButtonClicked() {
  auto sortedEvents = mCachedFeedEvents;
  const auto nowMs = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
  std::sort(sortedEvents.begin(), sortedEvents.end(),
            [nowMs](const DuckEvent &left, const DuckEvent &right) {
              return std::abs(left.start_date.value_or(0) - nowMs) <
                     std::abs(right.start_date.value_or(0) - nowMs);
            });

  QMenu menu(this);
  auto *clearAction = menu.addAction(tr("Don't link"));
  menu.addSeparator();
  QHash<QAction *, DuckEvent> actionToEvent;
  const auto count = std::min<qsizetype>(sortedEvents.size(), 8);
  for (qsizetype i = 0; i < count; ++i) {
    const auto &event = sortedEvents.at(i);
    const auto startAt = event.start_date.has_value()
                             ? QDateTime::fromMSecsSinceEpoch(*event.start_date,
                                                              QTimeZone::systemTimeZone())
                             : QDateTime{};
    const auto title = QString::fromStdString(event.name.value_or(tr("Session").toStdString()));
    auto *action = menu.addAction(QString("%1 · %2").arg(
        startAt.isValid() ? startAt.toString("dd.MM.yyyy HH:mm") : tr("Unknown time"), title));
    actionToEvent.insert(action, event);
  }

  const auto chosen = menu.exec(mLinkSessionButton->mapToGlobal(
      QPoint(0, mLinkSessionButton->height())));
  if (!chosen) {
    return;
  }

  mLinkManuallySet = true;
  if (chosen == clearAction) {
    mPendingLinkedEvent = std::nullopt;
  } else {
    mPendingLinkedEvent = actionToEvent.value(chosen);
  }
  updateLinkButtonText();
}
```

Add `#include <QMenu>` and `#include <QHash>` to the top of `client_notes_page.cpp`.

- [ ] **Step 5: Apply the pending link on save and reset state**

In `onAddNoteClicked()`, directly after `note.updated_at = note.created_at;` and before `const auto newNoteId = mDb->add_client_note(note);`:

```cpp
  if (mPendingLinkedEvent.has_value()) {
    if (mPendingLinkedEvent->is_virtual_occurrence) {
      note.linked_series_id = mPendingLinkedEvent->series_id;
      note.linked_occurrence_start_ms = mPendingLinkedEvent->original_occurrence_start;
    } else {
      note.linked_event_id = mPendingLinkedEvent->id;
    }
  }
```

Directly after `mPendingAttachments.clear();` (later in the same function, after the save succeeds), add:

```cpp
  mLinkManuallySet = false;
  mPendingLinkedEvent = nearestPastEvent(mCachedFeedEvents);
  updateLinkButtonText();
```

- [ ] **Step 6: Render the badge in `addNoteBubble` and emit `openEventRequested`**

In `addNoteBubble`, directly after the `timestampLabel` is added to `layout` and before the `bodyView` block, add:

```cpp
  if (mDb) {
    const auto linkedEvent = pcm::recurrence::resolveNoteLink(*mDb, note);
    if (linkedEvent.has_value() && linkedEvent->start_date.has_value()) {
      auto *linkLabel = new QPushButton(bubble);
      linkLabel->setFlat(true);
      linkLabel->setCursor(Qt::PointingHandCursor);
      const auto startAt = QDateTime::fromMSecsSinceEpoch(*linkedEvent->start_date,
                                                           QTimeZone::systemTimeZone());
      linkLabel->setText(QStringLiteral("🔗 %1").arg(startAt.toString("dd.MM.yyyy HH:mm")));
      linkLabel->setStyleSheet(
          "QPushButton { text-align: left; color: rgba(120, 170, 255, 0.9); "
          "background: transparent; border: none; padding: 0px; }");
      const auto linkedId = linkedEvent->id;
      const auto dayStartMs =
          QDateTime(startAt.date(), QTime(0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
      connect(linkLabel, &QPushButton::clicked, this, [this, linkedId, dayStartMs]() {
        emit openEventRequested(linkedId, dayStartMs);
      });
      layout->addWidget(linkLabel);
    }
  }
```

- [ ] **Step 7: Emit `openEventRequested` from session entries**

In `addSessionEntry`, directly after `timeLabel` is constructed (before it's added to `layout`), wrap it to be clickable — replace the plain `QLabel *timeLabel` construction with a clickable button that keeps the same look:

```cpp
  auto *timeLabel = new QPushButton(card);
  timeLabel->setFlat(true);
  timeLabel->setCursor(Qt::PointingHandCursor);
  timeLabel->setStyleSheet(
      "QPushButton { text-align: left; color: rgba(255, 255, 255, 0.90); "
      "font-weight: 600; background: transparent; border: none; padding: 0px; }");
```

(This replaces the two lines `auto *timeLabel = new QLabel(card);` and the old `timeLabel->setStyleSheet(...)` call further down — keep the `setText(...)` call as-is, just change the widget type and styling call site.)

After `layout->addWidget(timeLabel);`, add the click wiring:

```cpp
  const auto eventId = event.id;
  const auto dayStartMs =
      startAt.isValid()
          ? QDateTime(startAt.date(), QTime(0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch()
          : 0;
  connect(timeLabel, &QPushButton::clicked, this, [this, eventId, dayStartMs]() {
    emit openEventRequested(eventId, dayStartMs);
  });
```

- [ ] **Step 8: Build**

Run: `cmake --build build-release --target PsyClientManager_client_notes_page`
Expected: builds with no errors.

- [ ] **Step 9: Commit**

```bash
git add src/pages/client_notes_page/client_notes_page.h src/pages/client_notes_page/client_notes_page.cpp
git commit -m "feat(notes): link notes to sessions and make sessions clickable"
```

---

### Task 6: Jump-to-latest button

**Files:**
- Modify: `src/pages/client_notes_page/client_notes_page.h`
- Modify: `src/pages/client_notes_page/client_notes_page.cpp`

- [ ] **Step 1: Add the widget member**

In `client_notes_page.h`, add after `mScrollArea`:

```cpp
  QPushButton *mJumpToLatestButton = nullptr;
```

- [ ] **Step 2: Build and wire it in `buildUi()`**

In `client_notes_page.cpp`, directly after `mScrollArea->setWidget(mFeedWidget);` and before `feedSurfaceLayout->addWidget(mScrollArea);`:

```cpp
  mJumpToLatestButton = new QPushButton(tr("Jump to latest"), feedSurface);
  mJumpToLatestButton->setCursor(Qt::PointingHandCursor);
  mJumpToLatestButton->setVisible(false);
```

After `feedSurfaceLayout->addWidget(mScrollArea);`, add it as an overlay-style trailing row (simplest correct placement — a thin row anchored at the bottom of the feed surface, above the composer):

```cpp
  feedSurfaceLayout->addWidget(mJumpToLatestButton);
```

At the end of `buildUi()`, wire the scrollbar signals and the click handler:

```cpp
  connect(mScrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this,
          [this](const int value) {
            mJumpToLatestButton->setVisible(value < mScrollArea->verticalScrollBar()->maximum());
          });
  connect(mScrollArea->verticalScrollBar(), &QScrollBar::rangeChanged, this,
          [this](int, const int max) {
            mJumpToLatestButton->setVisible(mScrollArea->verticalScrollBar()->value() < max);
          });
  connect(mJumpToLatestButton, &QPushButton::clicked, this, [this]() {
    mScrollArea->verticalScrollBar()->setValue(mScrollArea->verticalScrollBar()->maximum());
  });
```

- [ ] **Step 3: Build**

Run: `cmake --build build-release --target PsyClientManager_client_notes_page`
Expected: builds with no errors.

- [ ] **Step 4: Commit**

```bash
git add src/pages/client_notes_page/client_notes_page.h src/pages/client_notes_page/client_notes_page.cpp
git commit -m "feat(notes): add a jump-to-latest button to the feed"
```

---

### Task 7: `MainWindow` + `QEventInfoPage` — event navigation wiring

**Files:**
- Modify: `src/pages/event_info_page/event_info.h`
- Modify: `src/pages/event_info_page/event_info.cpp`
- Modify: `src/app/main_window.cpp`

**Interfaces:**
- Consumes: `ClientNotesPage::openEventRequested` (Task 5).
- Produces: `QEventInfoPage::openEventOnDay(int64_t eventId, qint64 dayMs)`, consumed only by `MainWindow`.

- [ ] **Step 1: Declare `openEventOnDay`**

In `src/pages/event_info_page/event_info.h`, add to `public slots:` (directly after `refreshAppearance`):

```cpp
  void openEventOnDay(int64_t eventId, qint64 dayMs);
```

- [ ] **Step 2: Implement it**

In `src/pages/event_info_page/event_info.cpp`, add directly after `onCalendarClicked`:

```cpp
void QEventInfoPage::openEventOnDay(const int64_t eventId, const qint64 dayMs) {
  const auto date =
      QDateTime::fromMSecsSinceEpoch(dayMs, QTimeZone::systemTimeZone()).date();
  mCalendarWidget->setSelectedDate(date);
  mTimelineWidget->onSelectedDayChanged(date);
  onCalendarClicked(date);
  editEventWithDialog(eventId);
}
```

- [ ] **Step 3: Wire it in `MainWindow::connectSignals()`**

In `src/app/main_window.cpp`, directly after the two `openClientCardRequested` connections:

```cpp
  connect(clientNotesPage, &ClientNotesPage::openEventRequested,
          [this, eventInfoPage](const int64_t eventId, const qint64 dayMs) {
            eventInfoPage->openEventOnDay(eventId, dayMs);
            showPage(Pages::eventInfo, mBtnCalendar);
          });
```

- [ ] **Step 4: Build**

Run: `cmake --build build-release --target PsyClientManager_event_page PsyClientManager_app`
Expected: builds with no errors.

- [ ] **Step 5: Commit**

```bash
git add src/pages/event_info_page/event_info.h src/pages/event_info_page/event_info.cpp src/app/main_window.cpp
git commit -m "feat(notes): open the Calendar on the linked event when clicked"
```

---

### Task 8: Full build, smoke test, version bump, CHANGELOG

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/app/application.cpp`
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Full build**

Run: `cmake --build build-release`
Expected: builds with no errors.

- [ ] **Step 2: Run the full test suite**

Run: `ctest --test-dir build-release --output-on-failure`
Expected: all tests pass, including the new cases from Tasks 1-4.

- [ ] **Step 3: Manual smoke test**

```bash
./build-release/PsyClientManager &
APP_PID=$!
sleep 5
kill -0 $APP_PID && echo "still running" || echo "crashed"
kill $APP_PID
```

Expected: "still running". Then interactively: create a note and link it to a session via the composer button, confirm the badge appears and clicking it (or a session card) switches to Calendar on the right day with the event dialog open; scroll the feed up and confirm the jump-to-latest button appears and works.

- [ ] **Step 4: Bump version**

In `CMakeLists.txt`, change `VERSION 0.1.18` to `VERSION 0.1.19`.
In `src/app/application.cpp`, change `app.setApplicationVersion("0.1.18");` to `app.setApplicationVersion("0.1.19");`.

- [ ] **Step 5: Add CHANGELOG entry**

In `CHANGELOG.md`, add directly above the `## [0.1.18]` entry:

```markdown
## [0.1.19] - 2026-07-30

### Added

- Notes can now be linked to the session they're about, with a clickable
  badge that jumps to that event on the Calendar. Session entries in the
  timeline are clickable the same way. The feed also gained a
  jump-to-latest button.
```

- [ ] **Step 6: Rebuild to confirm the version bump compiles**

Run: `cmake --build build-release --target PsyClientManager`
Expected: builds with no errors.

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt src/app/application.cpp CHANGELOG.md
git commit -m "chore: bump version to 0.1.19"
```

---

## Self-Review

**Spec coverage:**
- Link storage (`linked_event_id` or `linked_series_id`+`linked_occurrence_start_ms`) — Task 1. ✓
- Link resolution surviving materialization — Task 3 (`resolveNoteLink`, with a dedicated "since materialized" test). ✓
- Composer link picker (button + popup, not `QComboBox`, defaults to nearest past session) — Task 5, uses `QMenu` (an explicitly allowed implementation choice per the design doc). ✓
- Note bubble session badge, clickable — Task 5 Step 6. ✓
- Session card clickable — Task 5 Step 7. ✓
- Full navigation (switch tab + select day + open event) — Task 7. ✓
- Jump-to-latest button — Task 6. ✓
- DB round-trip test for new columns — Task 1. ✓
- Backup/restore round-trip test — Task 4. ✓
- `resolveNoteLink` tests (materialized-by-id, virtual-unmaterialized, virtual-since-materialized) — Task 3. ✓
- Out-of-scope items (changing a link post-save, cascading UI on deleted links, system event-log) — not implemented anywhere in this plan. ✓

**Placeholder scan:** No TBD/TODO markers; every step has concrete code.

**Type consistency:** `resolveNoteLink(pcm::database::Database&, const DuckClientNote&)` (Task 3) matches its call site in Task 5 Step 6. `openEventRequested(int64_t, qint64)` (Task 5) matches `openEventOnDay(int64_t, qint64)` (Task 7) and the `connect(...)` lambda signature in Task 7 Step 3. `DuckClientNote::linked_event_id`/`linked_series_id`/`linked_occurrence_start_ms` (Task 1) match every read/write site in Tasks 3 and 5.

---

**Plan complete and saved to `docs/superpowers/plans/2026-07-30-note-session-linking.md`. Two execution options:**

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

**Which approach?**
