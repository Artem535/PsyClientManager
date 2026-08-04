# Event Change Log (Client Timeline System Lines) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** When a single event's status, payment status, or scheduled time changes, record it and show a small, clickable, neutral history line in that client's Notes feed (e.g. "Status changed: Scheduled → Completed"), interleaved chronologically with notes and session cards.

**Architecture:** `Database::update_event` diffs the old row against the incoming one and inserts one `EventChangeLog` row per changed aspect (status / payment / reschedule) on the same connection as the update. `ClientNotesPage` fetches these rows per client (joined through `EventClient`, same shape as existing event queries) and merges them into its existing sorted, filterable feed as a third `FeedItem` alternative, rendered as a small clickable centered line that jumps to the event on click — reusing the `openEventRequested` signal/navigation path that session cards and note badges already use.

**Tech Stack:** C++20, Qt6 Widgets, DuckDB (via the existing `pcm::database::Database` wrapper), GoogleTest.

## Global Constraints

- Every schema change is an idempotent `CREATE TABLE IF NOT EXISTS` / `ALTER TABLE ... ADD COLUMN IF NOT EXISTS` block in `src/database/constants.hpp` — this codebase has no separate migration files (see `Database::init_tables()` / `Database::apply_schema_migrations()`).
- Per `AGENTS.md`: "Database schema changes require a migration and a restore/round-trip test."
- Per `AGENTS.md`: raise `PROJECT_VERSION` in both `CMakeLists.txt` and `src/app/application.cpp`, and add a `CHANGELOG.md` entry, in the same MR as the schema change.
- Per `AGENTS.md`: production logs must stay free of note contents and other PII — do not `PLOG_*` the text of `cancellation_reason`.
- Display text is produced with Qt `tr()` at render time in C++; no raw Russian/English strings are ever persisted to the database (this is the exact concern behind issue #47).
- Only single-event edits (`Database::update_event`) are logged in this plan. Whole-series edits (`Database::update_event_series`) are explicitly out of scope (see the design doc's "Out of scope" section) — do not add logging there.
- This closes issue #57 (`Closes #57` in the final commit/PR — this is the second and final half of that issue; part 1 shipped in PR #58).

---

### Task 1: Database layer — schema, diff-and-log, retrieval

**Files:**
- Modify: `src/database/constants.hpp` (new table + new queries)
- Modify: `src/database/schema.hpp` (new `DuckEventChangeLog` struct)
- Modify: `src/database/database.h` (new method declaration)
- Modify: `src/database/database.cpp` (`update_event` diff logic, `remove_event` cleanup, new `get_event_change_log_for_client`)
- Test: `test/database_tests.cpp`

**Interfaces:**
- Consumes: existing `DuckEvent` (from `schema.hpp`), existing `Database::get_event(int64_t)`, existing `executePrepared`/`fkOrNull`/`timestampMsOrNull` helpers in `database.cpp`'s anonymous namespace.
- Produces: `DuckEventChangeLog` struct (in `schema.hpp`, namespace-less like `DuckEvent`), `std::vector<DuckEventChangeLog> Database::get_event_change_log_for_client(int64_t client_id)`. Task 2 and Task 3 both depend on this exact struct shape and method signature.

- [ ] **Step 1: Add the `EventChangeLog` table to `kCreateTables`**

In `src/database/constants.hpp`, insert immediately after the `ClientNoteAttachment` table's closing `);` (currently line 132, right before the closing `)duckdb";` of `kCreateTables`):

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

- [ ] **Step 2: Add the query constants**

In `src/database/constants.hpp`, add these three constants near the other event queries (right after `kSelectEventBySeriesOccurrenceQuery`, before `kDeleteEventClientByEventIdQuery`):

```sql
constexpr auto kInsertEventChangeLogQuery = R"duckdb(
INSERT INTO EventChangeLog (
    id, event_id, change_kind,
    old_event_stat_id, new_event_stat_id,
    old_payment_stat_id, new_payment_stat_id,
    old_start_date, new_start_date,
    cancellation_reason, occurred_at
)
SELECT COALESCE(MAX(id), 0) + 1, $1, $2, $3, $4, $5, $6, $7, $8, $9, $10
FROM EventChangeLog
RETURNING id
)duckdb";

constexpr auto kDeleteEventChangeLogByEventIdQuery =
    "DELETE FROM EventChangeLog WHERE event_id = $1";

constexpr auto kSelectEventChangeLogForClientQuery = R"duckdb(
SELECT ecl.*, e.start_date AS event_current_start_date
FROM EventChangeLog ecl
JOIN Event e ON e.id = ecl.event_id
JOIN EventClient ec ON ec.event_id = ecl.event_id
WHERE ec.client_id = $1
ORDER BY ecl.occurred_at ASC
)duckdb";
```

The extra `event_current_start_date` column (index 11, after the 11 `ecl.*` columns at indices 0-10) is the event's *current* start time — needed so a change-log line can always navigate to the right calendar day when clicked, even for status/payment rows that don't otherwise carry a date.

- [ ] **Step 3: Add the `DuckEventChangeLog` struct**

In `src/database/schema.hpp`, add this struct right after `DuckEventClient` (after its `operator<<`, before the `DuckClientNote` comment block):

```cpp
// --- DuckEventChangeLog ---
struct DuckEventChangeLog {
  std::int64_t id = -1;
  std::int64_t event_id = -1;
  std::int64_t change_kind = 0; // 1=status, 2=payment, 3=reschedule
  std::optional<std::int64_t> old_event_stat_id = std::nullopt;
  std::optional<std::int64_t> new_event_stat_id = std::nullopt;
  std::optional<std::int64_t> old_payment_stat_id = std::nullopt;
  std::optional<std::int64_t> new_payment_stat_id = std::nullopt;
  std::optional<std::int64_t> old_start_date = std::nullopt;
  std::optional<std::int64_t> new_start_date = std::nullopt;
  std::optional<std::string> cancellation_reason = std::nullopt;
  std::int64_t occurred_at = 0;
  std::optional<std::int64_t> event_current_start_date = std::nullopt;

  DuckEventChangeLog() = default;
  DuckEventChangeLog(const duckdb::DataChunk &chunk, duckdb::idx_t index) {
    id = db_utils::toInt32AsInt64(chunk.GetValue(0, index));
    event_id = db_utils::toInt32AsInt64(chunk.GetValue(1, index));
    change_kind = db_utils::toInt32AsInt64(chunk.GetValue(2, index));
    old_event_stat_id = db_utils::toOptionalInt32AsInt64(chunk.GetValue(3, index));
    new_event_stat_id = db_utils::toOptionalInt32AsInt64(chunk.GetValue(4, index));
    old_payment_stat_id = db_utils::toOptionalInt32AsInt64(chunk.GetValue(5, index));
    new_payment_stat_id = db_utils::toOptionalInt32AsInt64(chunk.GetValue(6, index));
    old_start_date = db_utils::toOptionalTimestampMs(chunk.GetValue(7, index));
    new_start_date = db_utils::toOptionalTimestampMs(chunk.GetValue(8, index));
    cancellation_reason = db_utils::toOptionalString(chunk.GetValue(9, index));
    occurred_at = db_utils::toOptionalTimestampMs(chunk.GetValue(10, index)).value_or(0);
    if (chunk.ColumnCount() > 11) {
      event_current_start_date =
          db_utils::toOptionalTimestampMs(chunk.GetValue(11, index));
    }
  }
};
inline std::ostream &operator<<(std::ostream &os, const DuckEventChangeLog &entry) {
  os << "DuckEventChangeLog{id=" << entry.id << ", event_id=" << entry.event_id
     << ", change_kind=" << entry.change_kind << ", occurred_at=" << entry.occurred_at
     << "}";
  return os;
}
```

The `if (chunk.ColumnCount() > 11)` guard follows the same forward-compatible-read pattern already used by `DuckEvent`/`DuckClientNote` in this file.

- [ ] **Step 4: Declare `get_event_change_log_for_client` in `database.h`**

In `src/database/database.h`, add this line right after the existing `std::vector<DuckEvent> get_events_for_client(int64_t client_id);` declaration:

```cpp
  std::vector<DuckEventChangeLog> get_event_change_log_for_client(int64_t client_id);
```

- [ ] **Step 5: Write the failing tests**

In `test/database_tests.cpp`, add these test cases (place them after `TEST(DatabaseTest, GetEventsForClientReturnsOnlyLinkedEvents)`):

```cpp
namespace {
std::pair<int64_t, int64_t> makeLinkedClientAndEvent(pcm::database::Database &db,
                                                      const int64_t startMs,
                                                      const int64_t endMs) {
  DuckClient client;
  client.name = std::string{"Change"};
  client.last_name = std::string{"Log"};
  const auto clientId = db.add_client(client);

  DuckEvent event;
  event.name = std::string{"Session"};
  event.start_date = startMs;
  event.end_date = endMs;
  event.event_stat_id = 1;
  event.payment_stat_id = 1;
  const auto eventId = db.add_event(event);
  db.add_event_client(eventId, clientId);
  return {clientId, eventId};
}
} // namespace

TEST(DatabaseTest, UpdateEventLogsStatusChange) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_changelog_status")}};
  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }
  pcm::database::Database db{conf};

  const auto [clientId, eventId] = makeLinkedClientAndEvent(db, 1730000000000, 1730003600000);

  DuckEvent updated;
  updated.id = eventId;
  updated.start_date = 1730000000000;
  updated.end_date = 1730003600000;
  updated.event_stat_id = 2;
  updated.payment_stat_id = 1;
  ASSERT_TRUE(db.update_event(updated));

  const auto entries = db.get_event_change_log_for_client(clientId);
  ASSERT_EQ(entries.size(), 1);
  EXPECT_EQ(entries.front().event_id, eventId);
  EXPECT_EQ(entries.front().change_kind, 1);
  ASSERT_TRUE(entries.front().old_event_stat_id.has_value());
  EXPECT_EQ(*entries.front().old_event_stat_id, 1);
  ASSERT_TRUE(entries.front().new_event_stat_id.has_value());
  EXPECT_EQ(*entries.front().new_event_stat_id, 2);
  EXPECT_FALSE(entries.front().old_payment_stat_id.has_value());
  EXPECT_FALSE(entries.front().old_start_date.has_value());

  db_dir.remove(true);
}

TEST(DatabaseTest, UpdateEventLogsPaymentStatusChange) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_changelog_payment")}};
  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }
  pcm::database::Database db{conf};

  const auto [clientId, eventId] = makeLinkedClientAndEvent(db, 1730000000000, 1730003600000);

  DuckEvent updated;
  updated.id = eventId;
  updated.start_date = 1730000000000;
  updated.end_date = 1730003600000;
  updated.event_stat_id = 1;
  updated.payment_stat_id = 2;
  ASSERT_TRUE(db.update_event(updated));

  const auto entries = db.get_event_change_log_for_client(clientId);
  ASSERT_EQ(entries.size(), 1);
  EXPECT_EQ(entries.front().change_kind, 2);
  ASSERT_TRUE(entries.front().old_payment_stat_id.has_value());
  EXPECT_EQ(*entries.front().old_payment_stat_id, 1);
  ASSERT_TRUE(entries.front().new_payment_stat_id.has_value());
  EXPECT_EQ(*entries.front().new_payment_stat_id, 2);

  db_dir.remove(true);
}

TEST(DatabaseTest, UpdateEventLogsReschedule) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_changelog_reschedule")}};
  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }
  pcm::database::Database db{conf};

  const auto [clientId, eventId] = makeLinkedClientAndEvent(db, 1730000000000, 1730003600000);

  DuckEvent updated;
  updated.id = eventId;
  updated.start_date = 1730100000000;
  updated.end_date = 1730103600000;
  updated.event_stat_id = 1;
  updated.payment_stat_id = 1;
  ASSERT_TRUE(db.update_event(updated));

  const auto entries = db.get_event_change_log_for_client(clientId);
  ASSERT_EQ(entries.size(), 1);
  EXPECT_EQ(entries.front().change_kind, 3);
  ASSERT_TRUE(entries.front().old_start_date.has_value());
  EXPECT_EQ(*entries.front().old_start_date, 1730000000000);
  ASSERT_TRUE(entries.front().new_start_date.has_value());
  EXPECT_EQ(*entries.front().new_start_date, 1730100000000);
  ASSERT_TRUE(entries.front().event_current_start_date.has_value());
  EXPECT_EQ(*entries.front().event_current_start_date, 1730100000000);

  db_dir.remove(true);
}

TEST(DatabaseTest, UpdateEventStatusChangeToCanceledIncludesReason) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_changelog_cancel")}};
  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }
  pcm::database::Database db{conf};

  const auto [clientId, eventId] = makeLinkedClientAndEvent(db, 1730000000000, 1730003600000);

  DuckEvent updated;
  updated.id = eventId;
  updated.start_date = 1730000000000;
  updated.end_date = 1730003600000;
  updated.event_stat_id = 3;
  updated.payment_stat_id = 1;
  updated.cancellation_reason = std::string{"Client request"};
  ASSERT_TRUE(db.update_event(updated));

  const auto entries = db.get_event_change_log_for_client(clientId);
  ASSERT_EQ(entries.size(), 1);
  EXPECT_EQ(entries.front().change_kind, 1);
  ASSERT_TRUE(entries.front().cancellation_reason.has_value());
  EXPECT_EQ(*entries.front().cancellation_reason, "Client request");

  db_dir.remove(true);
}

TEST(DatabaseTest, UpdateEventNoOpProducesNoChangeLogRows) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_changelog_noop")}};
  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }
  pcm::database::Database db{conf};

  const auto [clientId, eventId] = makeLinkedClientAndEvent(db, 1730000000000, 1730003600000);

  DuckEvent updated;
  updated.id = eventId;
  updated.start_date = 1730000000000;
  updated.end_date = 1730003600000;
  updated.event_stat_id = 1;
  updated.payment_stat_id = 1;
  ASSERT_TRUE(db.update_event(updated));

  EXPECT_TRUE(db.get_event_change_log_for_client(clientId).empty());

  db_dir.remove(true);
}

TEST(DatabaseTest, UpdateEventCombinedChangeProducesOneRowPerAspect) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_changelog_combined")}};
  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }
  pcm::database::Database db{conf};

  const auto [clientId, eventId] = makeLinkedClientAndEvent(db, 1730000000000, 1730003600000);

  DuckEvent updated;
  updated.id = eventId;
  updated.start_date = 1730100000000;
  updated.end_date = 1730103600000;
  updated.event_stat_id = 2;
  updated.payment_stat_id = 2;
  ASSERT_TRUE(db.update_event(updated));

  const auto entries = db.get_event_change_log_for_client(clientId);
  ASSERT_EQ(entries.size(), 3);
  std::set<int64_t> kinds;
  for (const auto &entry : entries) {
    kinds.insert(entry.change_kind);
  }
  EXPECT_EQ(kinds, (std::set<int64_t>{1, 2, 3}));

  db_dir.remove(true);
}

TEST(DatabaseTest, RemoveEventSucceedsAfterChangeLogRowsExist) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_changelog_remove")}};
  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }
  pcm::database::Database db{conf};

  const auto [clientId, eventId] = makeLinkedClientAndEvent(db, 1730000000000, 1730003600000);

  DuckEvent updated;
  updated.id = eventId;
  updated.start_date = 1730000000000;
  updated.end_date = 1730003600000;
  updated.event_stat_id = 2;
  updated.payment_stat_id = 1;
  ASSERT_TRUE(db.update_event(updated));
  ASSERT_EQ(db.get_event_change_log_for_client(clientId).size(), 1);

  EXPECT_TRUE(db.remove_event(eventId));

  db_dir.remove(true);
}
```

`test/database_tests.cpp` doesn't currently `#include <set>` — add `#include <set>` to its include block at the top of the file (needed for `UpdateEventCombinedChangeProducesOneRowPerAspect`).

- [ ] **Step 6: Run the tests to verify they fail**

```bash
cmake -S . -B build -DPCM_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure -R DatabaseTest
```

Expected: compile failure (`DuckEventChangeLog`/`get_event_change_log_for_client` don't exist yet) or, once compiling against the new declarations, `EXPECT_EQ(entries.size(), 1)`-style failures with `entries.size() == 0` because `update_event` doesn't write any log rows yet.

- [ ] **Step 7: Implement the diff-and-log logic in `update_event`**

In `src/database/database.cpp`, modify `Database::update_event`. Right after `duckdb::Connection conn(*mDb);` (the very first line of the function body), add:

```cpp
  std::unique_ptr<DuckEvent> existingEvent;
  auto existingResult = executePrepared(conn, constance::kSelectEventByIdQuery,
                                        {duckdb::Value::BIGINT(event.id)});
  if (existingResult && !existingResult->HasError()) {
    if (auto chunk = existingResult->Fetch(); chunk && chunk->size() > 0) {
      existingEvent = std::make_unique<DuckEvent>(*chunk, 0);
    }
  }
```

Then, right before the function's final `return true;` (i.e. after the existing `if (!result || result->HasError()) { ... return false; }` block, once the update has succeeded), add:

```cpp
  if (existingEvent) {
    const auto nowMs = Poco::Timestamp().epochMicroseconds() / 1000;
    const auto occurredAt = db_utils::toDuckTimestamp(nowMs * 1000);
    const auto effectiveNewEventStat =
        event.event_stat_id > 0 ? event.event_stat_id : existingEvent->event_stat_id;
    const auto effectiveNewPaymentStat =
        event.payment_stat_id > 0 ? event.payment_stat_id : existingEvent->payment_stat_id;

    if (effectiveNewEventStat != existingEvent->event_stat_id) {
      const auto reasonValue = effectiveNewEventStat == 3
                                    ? db_utils::toDuckValue(event.cancellation_reason)
                                    : duckdb::Value();
      executePrepared(
          conn, constance::kInsertEventChangeLogQuery,
          {duckdb::Value::BIGINT(event.id), duckdb::Value::INTEGER(1),
           duckdb::Value::INTEGER(static_cast<int32_t>(existingEvent->event_stat_id)),
           duckdb::Value::INTEGER(static_cast<int32_t>(effectiveNewEventStat)),
           duckdb::Value(), duckdb::Value(), duckdb::Value(), duckdb::Value(),
           reasonValue, occurredAt});
    }

    if (effectiveNewPaymentStat != existingEvent->payment_stat_id) {
      executePrepared(
          conn, constance::kInsertEventChangeLogQuery,
          {duckdb::Value::BIGINT(event.id), duckdb::Value::INTEGER(2), duckdb::Value(),
           duckdb::Value(),
           duckdb::Value::INTEGER(static_cast<int32_t>(existingEvent->payment_stat_id)),
           duckdb::Value::INTEGER(static_cast<int32_t>(effectiveNewPaymentStat)),
           duckdb::Value(), duckdb::Value(), duckdb::Value(), occurredAt});
    }

    if (event.start_date.value_or(0) != existingEvent->start_date.value_or(0)) {
      executePrepared(
          conn, constance::kInsertEventChangeLogQuery,
          {duckdb::Value::BIGINT(event.id), duckdb::Value::INTEGER(3), duckdb::Value(),
           duckdb::Value(), duckdb::Value(), duckdb::Value(),
           timestampMsOrNull(existingEvent->start_date), timestampMsOrNull(event.start_date),
           duckdb::Value(), occurredAt});
    }
  }

  return true;
```

This uses the already-declared `db_utils`, `executePrepared`, `timestampMsOrNull` — no new includes needed (`<Poco/Timestamp.h>` is already available transitively the same way `add_event_series` uses `Poco::Timestamp` in this same file).

- [ ] **Step 8: Add `remove_event` cleanup**

In `src/database/database.cpp`, in `Database::remove_event`, right after the existing `EventClient` deletion block (after the `if (!relationResult || relationResult->HasError()) { ... }` check) and before the `Event` row deletion, add:

```cpp
  auto changeLogResult = executePrepared(
      conn, constance::kDeleteEventChangeLogByEventIdQuery, {duckdb::Value::BIGINT(id)});
  if (!changeLogResult || changeLogResult->HasError()) {
    PLOG_ERROR << "Failed to delete EventChangeLog rows for event (id=" << id
               << "): " << changeLogResult->GetError();
    return false;
  }
```

- [ ] **Step 9: Implement `get_event_change_log_for_client`**

In `src/database/database.cpp`, add this right after `Database::get_events_for_client` (before `Database::get_client_notes`):

```cpp
std::vector<DuckEventChangeLog>
Database::get_event_change_log_for_client(const int64_t client_id) {
  if (client_id <= 0) {
    return {};
  }

  duckdb::Connection conn(*mDb);
  auto result = executePrepared(conn, constance::kSelectEventChangeLogForClientQuery,
                                {duckdb::Value::BIGINT(client_id)});
  if (!result || result->HasError()) {
    PLOG_ERROR << "Failed to fetch event change log for client_id=" << client_id << ": "
               << (result ? result->GetError() : "prepare failed");
    return {};
  }

  std::vector<DuckEventChangeLog> entries;
  while (auto chunk = result->Fetch()) {
    for (duckdb::idx_t i = 0; i < chunk->size(); ++i) {
      entries.emplace_back(*chunk, i);
    }
  }

  return entries;
}
```

- [ ] **Step 10: Run the tests to verify they pass**

```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure -R DatabaseTest
```

Expected: all `DatabaseTest.*` cases pass, including the 7 new ones.

- [ ] **Step 11: Commit**

```bash
git add src/database/constants.hpp src/database/schema.hpp src/database/database.h \
        src/database/database.cpp test/database_tests.cpp
git commit -m "feat(db): log event status/payment/reschedule changes to EventChangeLog"
```

---

### Task 2: Backup/restore round-trip coverage

**Files:**
- Test: `test/backup_tests.cpp`

**Interfaces:**
- Consumes: `Database::update_event`, `Database::get_event_change_log_for_client` (Task 1), the existing `makeTestDatabase` test helper already in `test/backup_tests.cpp`, `pcm::backup::BackupService`/`RestoreService`.

- [ ] **Step 1: Write the failing test**

In `test/backup_tests.cpp`, add this test right after `TEST(RestoreServiceTest, RestoresLinkedNoteFields)`:

```cpp
TEST(RestoreServiceTest, RestoresEventChangeLogRows) {
  auto sourceDb = makeTestDatabase("tmp_restore_changelog_source");

  DuckClient client;
  client.name = std::string{"Jack"};
  client.last_name = std::string{"J"};
  const auto clientId = sourceDb.add_client(client);
  ASSERT_GT(clientId, 0);

  DuckEvent event;
  event.name = std::string{"Session"};
  event.start_date = 1730000000000;
  event.end_date = 1730003600000;
  event.event_stat_id = 1;
  event.payment_stat_id = 1;
  const auto eventId = sourceDb.add_event(event);
  ASSERT_GT(eventId, 0);
  ASSERT_GT(sourceDb.add_event_client(eventId, clientId), 0);

  DuckEvent updated;
  updated.id = eventId;
  updated.start_date = 1730000000000;
  updated.end_date = 1730003600000;
  updated.event_stat_id = 2;
  updated.payment_stat_id = 2;
  ASSERT_TRUE(sourceDb.update_event(updated));
  ASSERT_EQ(sourceDb.get_event_change_log_for_client(clientId).size(), 2);

  const auto backupPath = Poco::Path(Poco::Path::current())
                              .append("tmp_restore_changelog.psybackup")
                              .toString();
  if (Poco::File(backupPath).exists()) {
    Poco::File(backupPath).remove();
  }
  ASSERT_TRUE(pcm::backup::BackupService{}.create_backup(sourceDb, backupPath).ok);

  const auto targetPath = Poco::Path(Poco::Path::current())
                              .append("tmp_restore_changelog_target")
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
  const auto entries = restoredDb.get_event_change_log_for_client(clientId);
  ASSERT_EQ(entries.size(), 2);

  Poco::File(backupPath).remove();
  Poco::File(targetPath).remove(true);
  Poco::File(Poco::Path(Poco::Path::current()).append("tmp_restore_changelog_source"))
      .remove(true);
}
```

- [ ] **Step 2: Run the test to verify it fails**

```bash
cmake --build build --parallel
ctest --test-dir build --output-on-failure -R RestoresEventChangeLogRows
```

Expected: PASS is *not* expected here if Task 1 is already merged into this branch — since `EXPORT DATABASE`/`IMPORT DATABASE` covers the whole schema automatically, this test should already pass once Task 1 is done. Run it to confirm that's actually true (it validates Task 1's schema addition is wired into `kCreateTables` correctly, not just declared).

- [ ] **Step 3: If it fails, fix forward; if it passes, proceed**

If this fails, the most likely cause is `EventChangeLog` missing from `kCreateTables` (Task 1, Step 1) or `RETURNING id` mismatch in the insert query. Re-check those before writing new code — do not add restore-specific special-casing; `EXPORT DATABASE`/`IMPORT DATABASE` is schema-agnostic by design and no table-specific backup code should be needed.

- [ ] **Step 4: Commit**

```bash
git add test/backup_tests.cpp
git commit -m "test(backup): cover EventChangeLog round-trip through backup/restore"
```

---

### Task 3: Client Notes feed integration

**Files:**
- Modify: `src/pages/client_notes_page/client_notes_page.h`
- Modify: `src/pages/client_notes_page/client_notes_page.cpp`

**Interfaces:**
- Consumes: `Database::get_event_change_log_for_client(int64_t)` and `DuckEventChangeLog` (Task 1), existing `ClientNotesPage::openEventRequested(int64_t eventId, qint64 dayMs)` signal (unchanged — no new signal needed), existing `mFeedLayout`/`mFeedWidget`/`FeedFilter` members.
- Produces: `ClientNotesPage::addChangeLogEntry(const DuckEventChangeLog&)` — no other task depends on this; it's the final leaf of the feature.

- [ ] **Step 1: Declare `addChangeLogEntry` in the header**

In `src/pages/client_notes_page/client_notes_page.h`, add this line right after `void addSessionEntry(const DuckEvent &event);`:

```cpp
  void addChangeLogEntry(const DuckEventChangeLog &entry);
```

- [ ] **Step 2: Add status/payment label helpers**

In `src/pages/client_notes_page/client_notes_page.cpp`, inside the existing anonymous namespace (the one containing `makeSurface`, near the top of the file), add these two functions:

```cpp
QString eventChangeStatusLabel(const int64_t eventStatusId) {
  switch (eventStatusId) {
  case 2:
    return ClientNotesPage::tr("Completed");
  case 3:
    return ClientNotesPage::tr("Canceled");
  case 4:
    return ClientNotesPage::tr("Confirmed");
  case 5:
    return ClientNotesPage::tr("No show");
  case 6:
    return ClientNotesPage::tr("Rescheduled");
  case 1:
  default:
    return ClientNotesPage::tr("Scheduled");
  }
}

QString eventChangePaymentLabel(const int64_t paymentStatusId) {
  switch (paymentStatusId) {
  case 2:
    return ClientNotesPage::tr("Paid");
  case 3:
    return ClientNotesPage::tr("Canceled");
  case 4:
    return ClientNotesPage::tr("Refunded");
  case 5:
    return ClientNotesPage::tr("Skipped");
  case 1:
  default:
    return ClientNotesPage::tr("Pending");
  }
}
```

These mirror `eventStatusLabel`/`paymentStatusLabel` in `src/event_view/event_item.cpp` exactly (same `tr()` source strings) so Qt Linguist reuses the existing RU translations instead of requiring new ones.

- [ ] **Step 3: Implement `addChangeLogEntry`**

In `src/pages/client_notes_page/client_notes_page.cpp`, add this method right after `ClientNotesPage::addDateDivider`:

```cpp
void ClientNotesPage::addChangeLogEntry(const DuckEventChangeLog &entry) {
  QString text;
  switch (entry.change_kind) {
  case 1: {
    text = tr("Status changed: %1 → %2")
               .arg(eventChangeStatusLabel(entry.old_event_stat_id.value_or(1)),
                    eventChangeStatusLabel(entry.new_event_stat_id.value_or(1)));
    if (entry.new_event_stat_id.value_or(0) == 3 && entry.cancellation_reason.has_value() &&
        !entry.cancellation_reason->empty()) {
      text += QStringLiteral(" (%1)").arg(QString::fromStdString(*entry.cancellation_reason));
    }
    break;
  }
  case 2:
    text = tr("Payment status changed: %1 → %2")
               .arg(eventChangePaymentLabel(entry.old_payment_stat_id.value_or(1)),
                    eventChangePaymentLabel(entry.new_payment_stat_id.value_or(1)));
    break;
  case 3: {
    const auto oldAt = entry.old_start_date.has_value()
                            ? QDateTime::fromMSecsSinceEpoch(*entry.old_start_date,
                                                             QTimeZone::systemTimeZone())
                            : QDateTime{};
    const auto newAt = entry.new_start_date.has_value()
                            ? QDateTime::fromMSecsSinceEpoch(*entry.new_start_date,
                                                             QTimeZone::systemTimeZone())
                            : QDateTime{};
    text = tr("Rescheduled from %1 to %2")
               .arg(oldAt.isValid() ? oldAt.toString("dd.MM.yyyy HH:mm") : tr("Unknown time"),
                    newAt.isValid() ? newAt.toString("dd.MM.yyyy HH:mm") : tr("Unknown time"));
    break;
  }
  default:
    return;
  }

  auto *line = new QPushButton(text, mFeedWidget);
  line->setFlat(true);
  line->setCursor(Qt::PointingHandCursor);
  line->setStyleSheet(
      "QPushButton { color: rgba(255, 255, 255, 0.45); background: transparent; "
      "border: none; padding: 2px 0px; }"
      "QPushButton:hover { color: rgba(255, 255, 255, 0.65); }");

  const auto eventId = entry.event_id;
  qint64 dayStartMs = 0;
  if (entry.event_current_start_date.has_value()) {
    const auto startAt = QDateTime::fromMSecsSinceEpoch(*entry.event_current_start_date,
                                                         QTimeZone::systemTimeZone());
    dayStartMs =
        QDateTime(startAt.date(), QTime(0, 0), QTimeZone::systemTimeZone()).toMSecsSinceEpoch();
  }
  connect(line, &QPushButton::clicked, this, [this, eventId, dayStartMs]() {
    emit openEventRequested(eventId, dayStartMs);
  });

  mFeedLayout->insertWidget(mFeedLayout->count() - 1, line, 0, Qt::AlignCenter);
}
```

- [ ] **Step 4: Wire change-log entries into `reloadNotes`**

In `src/pages/client_notes_page/client_notes_page.cpp`, in `ClientNotesPage::reloadNotes`:

a) Right after the existing block that builds `events` (right after `updateAppointmentSummary(events);` and before `mCachedFeedEvents = events;`), add:

```cpp
  const auto changeLogEntries =
      mDb ? mDb->get_event_change_log_for_client(mCurrentClient->id)
          : std::vector<DuckEventChangeLog>{};
```

b) Change the `FeedItem` alias from:

```cpp
  using FeedItem = std::variant<DuckClientNote, DuckEvent>;
```

to:

```cpp
  using FeedItem = std::variant<DuckClientNote, DuckEvent, DuckEventChangeLog>;
```

c) Change the item-population block from:

```cpp
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
```

to:

```cpp
  if (mFeedFilter != FeedFilter::Sessions) {
    for (const auto &note : notes) {
      items.emplace_back(note);
    }
  }
  if (mFeedFilter != FeedFilter::Notes) {
    for (const auto &event : events) {
      items.emplace_back(event);
    }
    for (const auto &entry : changeLogEntries) {
      items.emplace_back(entry);
    }
  }
```

(Change-log lines show under `All` and `Sessions`, hidden under `Notes` — same visibility rule as session cards.)

d) Change the `timestampOf` lambda from:

```cpp
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
```

to:

```cpp
  const auto timestampOf = [](const FeedItem &item) -> qint64 {
    return std::visit(
        [](const auto &value) -> qint64 {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, DuckClientNote>) {
            return value.created_at.value_or(0);
          } else if constexpr (std::is_same_v<T, DuckEventChangeLog>) {
            return value.occurred_at;
          } else {
            return value.start_date.value_or(0);
          }
        },
        item);
  };
```

e) Change the render-dispatch `std::visit` from:

```cpp
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
```

to:

```cpp
    std::visit(
        [this](const auto &value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, DuckClientNote>) {
            addNoteBubble(value);
          } else if constexpr (std::is_same_v<T, DuckEventChangeLog>) {
            addChangeLogEntry(value);
          } else {
            addSessionEntry(value);
          }
        },
        item);
```

- [ ] **Step 5: Build**

```bash
cmake --build build-release --parallel
```

Expected: builds cleanly. This page has no dedicated unit test target (`ClientNotesPage` is pure Qt widget code with no existing test harness in `test/`), so verification here is build success plus the manual check in Step 6.

- [ ] **Step 6: Manual smoke test**

```bash
./build-release/PsyClientManager &
```

In the app: open a client's Notes page, note the current entry count/order. Switch to the Calendar page, open one of that client's sessions, change its status (e.g. Scheduled → Completed) and save. Return to Notes: a new centered line "Status changed: Scheduled → Completed" should appear in chronological position (today), visible under both `All` and `Sessions` filters, hidden under `Notes`. Click the line — it should jump to that event on the Calendar. Repeat once for a payment-status change and once for a time change (drag or edit the event's time) to confirm all three message shapes render correctly. Kill the app afterward (`kill %1` or close the window).

- [ ] **Step 7: Commit**

```bash
git add src/pages/client_notes_page/client_notes_page.h src/pages/client_notes_page/client_notes_page.cpp
git commit -m "feat(notes): show event status/payment/reschedule history lines in the client feed"
```

---

### Task 4: Version, changelog, and issue closure

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/app/application.cpp`
- Modify: `CHANGELOG.md`

**Interfaces:**
- Consumes: nothing from earlier tasks structurally; this is the release-bookkeeping task that always runs last per `AGENTS.md`.
- Produces: nothing consumed elsewhere.

- [ ] **Step 1: Bump the version**

In `CMakeLists.txt`, find the current `project(PsyClientManager VERSION X.Y.Z LANGUAGES CXX)` line and increment the patch number by one from whatever it is at the time this task runs (check with `git log -1 -- CMakeLists.txt` or just read the current file — do not assume a specific number, since Tasks 1-3 may land after other version bumps merge to `main` first).

In `src/app/application.cpp`, update `app.setApplicationVersion("X.Y.Z");` to the same new version string.

- [ ] **Step 2: Update the changelog**

In `CHANGELOG.md`, add a new entry above the current top-most `## [X.Y.Z]` entry, matching the new version from Step 1:

```markdown
## [X.Y.Z] - 2026-08-01

### Added

- The client Notes feed now shows a small history line whenever a session's
  status, payment status, or scheduled time changes (e.g. "Status changed:
  Scheduled → Completed"), interleaved chronologically with notes and
  session cards. Click a line to jump to that session on the Calendar.
```

- [ ] **Step 3: Build and run the full test suite**

```bash
cmake --build build-release --parallel
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Expected: full green build, all tests pass (existing suite + the new cases from Tasks 1-2).

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt src/app/application.cpp CHANGELOG.md
git commit -m "$(cat <<'EOF'
chore: bump version and changelog for event change-log feed

Closes #57.
EOF
)"
```
