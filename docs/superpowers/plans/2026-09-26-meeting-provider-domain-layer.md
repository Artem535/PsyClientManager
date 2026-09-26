# Meeting Provider Domain Layer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give `Event`/`EventSeries` a provider-agnostic way to carry an online meeting — `provider_kind`, an opaque `meeting_ref`, and `invitation_state` — behind a polymorphic `MeetingProvider` interface, with a fully working `ExternalUrlMeetingProvider` (today's free-text-link behavior) and a non-functional `LiveKitMeetingProvider` stub that satisfies the same interface without making any network call.

**Architecture:** A new `src/meeting` library defines `ProviderKind`, `MeetingDescriptor`, the async signal-based `MeetingProvider` interface (modeled on `CredentialStore`), its two implementations, and `MeetingCoordinator` — a non-Qt-UI class that dispatches to the right provider and is the only thing `QEventDetailsWidget` and `QTimelineModel` talk to. `Event`/`EventSeries` gain three new nullable columns, backfilled for existing `is_online=true` rows. The UI does not change: the practitioner still sees only the existing "Online session" toggle and link field; under the hood this now always resolves to `ProviderKind::ExternalUrl`. `LiveKit` is wired into every layer (schema, interface, coordinator) but is unreachable from the UI and never selected — issue #80 is what makes it real and selectable.

**Tech Stack:** C++20, Qt6 (Core, Widgets), DuckDB, GoogleTest, this repo's existing `pcm::database::Database`/`DuckEvent`/`DuckEventSeries` layer.

## Global Constraints

- No UI changes: `QEventDetailsWidget`'s "Online session" toggle and link field behave exactly as before. `LiveKit` must not become selectable anywhere in this plan.
- `MeetingProvider::create`/`cancel` are asynchronous and signal-based (see `docs/asciidoc/13-meeting-provider-interface-adr.adoc`), even though `ExternalUrlMeetingProvider` does no I/O and `LiveKitMeetingProvider` is a stub. Do not make them return values synchronously.
- `LiveKitMeetingProvider` must never claim success. Its `create`/`cancel` always report failure via `createFailed`/`cancelFailed` — never fabricate a `MeetingDescriptor`.
- New DB columns are nullable (`provider_kind TEXT`, `meeting_ref TEXT`, `invitation_state TEXT`) on both `Event` and `EventSeries`, added the same way every other column in this schema was added: `ALTER TABLE ... ADD COLUMN IF NOT EXISTS ...` in `kSchemaMigrations`. Existing rows with `is_online = TRUE` are explicitly backfilled to `provider_kind = 'ExternalUrl'`.
- Follow existing repo conventions throughout: `PascalCase` for classes, `camelCase` for Qt methods, `snake_case` for DB/schema fields (per `AGENTS.md`).
- Every MR requires: version bump in both `CMakeLists.txt` and `src/app/application.cpp`, and a `CHANGELOG.md` entry (per `AGENTS.md`). Current version is `0.1.32`.

---

### Task 1: `Event`/`EventSeries` schema — `provider_kind`, `meeting_ref`, `invitation_state`

**Files:**
- Modify: `src/database/constants.hpp` (`kCreateTables`, `kSchemaMigrations`, `kInsertEventQuery`, `kUpdateEventQuery`, `kInsertEventSeriesQuery`, `kUpdateEventSeriesQuery`)
- Modify: `src/database/schema.hpp` (`DuckEvent`, `DuckEventSeries`)
- Modify: `src/database/database.cpp` (`Database::add_event`, `Database::update_event`, `Database::add_event_series`, `Database::update_event_series`)
- Test: `test/database_tests.cpp`

**Interfaces:**
- Produces: `DuckEvent::provider_kind`, `DuckEvent::meeting_ref`, `DuckEvent::invitation_state` (all `std::optional<std::string>`); the identical trio on `DuckEventSeries`. Every later task reads/writes these exact field names and type.

- [ ] **Step 1: Write the failing tests**

Add to `test/database_tests.cpp` (append near the other `Add`/`Update` tests, e.g. after `HandlesNullBufferColumnsFromLegacyDatabase`):

```cpp
TEST(DatabaseTest, PersistsProviderFieldsOnEvent) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_provider_event")}};
  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};
  DuckEvent event;
  event.name = std::string{"Online Session"};
  event.start_date = 1730000000000;
  event.end_date = 1730003600000;
  event.is_online = true;
  event.meeting_url = "https://meet.example.invalid/room-1";
  event.provider_kind = std::string{"ExternalUrl"};
  event.meeting_ref = std::string{"https://meet.example.invalid/room-1"};
  event.invitation_state = std::nullopt;

  const auto eventId = db.add_event(event);
  ASSERT_GT(eventId, 0);

  const auto reloaded = db.get_event(eventId);
  ASSERT_NE(reloaded, nullptr);
  ASSERT_TRUE(reloaded->provider_kind.has_value());
  EXPECT_EQ(*reloaded->provider_kind, "ExternalUrl");
  ASSERT_TRUE(reloaded->meeting_ref.has_value());
  EXPECT_EQ(*reloaded->meeting_ref, "https://meet.example.invalid/room-1");
  EXPECT_FALSE(reloaded->invitation_state.has_value());

  reloaded->invitation_state = std::string{"pending"};
  ASSERT_TRUE(db.update_event(*reloaded));
  const auto updated = db.get_event(eventId);
  ASSERT_NE(updated, nullptr);
  ASSERT_TRUE(updated->invitation_state.has_value());
  EXPECT_EQ(*updated->invitation_state, "pending");

  db_dir.remove(true);
}

TEST(DatabaseTest, PersistsProviderFieldsOnEventSeries) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_provider_series")}};
  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};
  DuckEventSeries series;
  series.name = std::string{"Weekly Online Session"};
  series.start_date = 1730000000000;
  series.end_date = 1730003600000;
  series.duration = 3600;
  series.recurrence_rule = "FREQ=WEEKLY;INTERVAL=1";
  series.is_online = true;
  series.meeting_url = "https://meet.example.invalid/room-2";
  series.provider_kind = std::string{"ExternalUrl"};
  series.meeting_ref = std::string{"https://meet.example.invalid/room-2"};

  const auto seriesId = db.add_event_series(series);
  ASSERT_GT(seriesId, 0);

  const auto reloaded = db.get_event_series(seriesId);
  ASSERT_NE(reloaded, nullptr);
  ASSERT_TRUE(reloaded->provider_kind.has_value());
  EXPECT_EQ(*reloaded->provider_kind, "ExternalUrl");
  ASSERT_TRUE(reloaded->meeting_ref.has_value());
  EXPECT_EQ(*reloaded->meeting_ref, "https://meet.example.invalid/room-2");

  db_dir.remove(true);
}

TEST(DatabaseTest, BackfillsProviderKindForLegacyOnlineEvents) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_provider_backfill")}};
  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  int64_t eventId = 0;
  {
    pcm::database::Database db{conf};
    DuckEvent event;
    event.name = std::string{"Legacy Online Event"};
    event.start_date = 1730000000000;
    event.end_date = 1730003600000;
    event.is_online = true;
    event.meeting_url = "https://legacy.example.invalid/room";
    eventId = db.add_event(event);
    ASSERT_GT(eventId, 0);
  }

  {
    // Simulate a pre-existing row written before provider_kind existed.
    duckdb::DuckDB rawDatabase((conf.db_conf().db_pth.toString() + "/database.db").c_str());
    duckdb::Connection rawConnection(rawDatabase);
    ASSERT_FALSE(rawConnection
                     .Query("UPDATE Event SET provider_kind = NULL WHERE id = " +
                            std::to_string(eventId))
                     ->HasError());
  }

  // Re-opening the database re-runs schema migrations, which must backfill provider_kind.
  pcm::database::Database db{conf};
  const auto reloaded = db.get_event(eventId);
  ASSERT_NE(reloaded, nullptr);
  ASSERT_TRUE(reloaded->provider_kind.has_value());
  EXPECT_EQ(*reloaded->provider_kind, "ExternalUrl");

  db_dir.remove(true);
}
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake -S . -B build -DPCM_BUILD_TESTS=ON && cmake --build build --target Sessio_database_tests --parallel`
Expected: compilation failure — `DuckEvent`/`DuckEventSeries` have no member named `provider_kind`.

- [ ] **Step 3: Add the columns and migrations**

In `src/database/constants.hpp`, extend `kCreateTables` — the `Event` table (currently ending at `buffer_after_minutes INTEGER DEFAULT 0`):

```cpp
CREATE TABLE IF NOT EXISTS Event (
    id INTEGER PRIMARY KEY,
    name TEXT,
    description TEXT,
    is_work_event BOOLEAN,
    event_stat_id INTEGER REFERENCES EventStatus(id),
    payment_stat_id INTEGER REFERENCES PaymentStatus(id),
    start_date TIMESTAMP,
    end_date TIMESTAMP,
    duration INTEGER,
    cost DOUBLE,
    reminder_notified_at TIMESTAMP,
    is_online BOOLEAN DEFAULT FALSE,
    meeting_url TEXT,
    series_id INTEGER,
    original_occurrence_start TIMESTAMP,
    cancellation_reason TEXT,
    canceled_by TEXT,
    buffer_before_minutes INTEGER DEFAULT 0,
    buffer_after_minutes INTEGER DEFAULT 0,
    provider_kind TEXT,
    meeting_ref TEXT,
    invitation_state TEXT
);

CREATE TABLE IF NOT EXISTS EventSeries (
    id INTEGER PRIMARY KEY,
    name TEXT,
    description TEXT,
    client_id INTEGER REFERENCES Client(id),
    is_work_event BOOLEAN,
    event_stat_id INTEGER REFERENCES EventStatus(id),
    payment_stat_id INTEGER REFERENCES PaymentStatus(id),
    start_date TIMESTAMP,
    end_date TIMESTAMP,
    duration INTEGER,
    cost DOUBLE,
    is_online BOOLEAN DEFAULT FALSE,
    meeting_url TEXT,
    recurrence_rule TEXT NOT NULL,
    recurrence_until TIMESTAMP,
    active BOOLEAN DEFAULT TRUE,
    created_at TIMESTAMP,
    updated_at TIMESTAMP,
    cancellation_reason TEXT,
    canceled_by TEXT,
    buffer_before_minutes INTEGER DEFAULT 0,
    buffer_after_minutes INTEGER DEFAULT 0,
    provider_kind TEXT,
    meeting_ref TEXT,
    invitation_state TEXT
);
```

(Only these two `CREATE TABLE` blocks change — every other table in `kCreateTables` stays as-is.)

Append to `kSchemaMigrations` (after the existing `UPDATE EventSeries SET buffer_after_minutes = 0 ...` line, before the `EventSeriesException` block):

```cpp
ALTER TABLE Event ADD COLUMN IF NOT EXISTS provider_kind TEXT;
ALTER TABLE Event ADD COLUMN IF NOT EXISTS meeting_ref TEXT;
ALTER TABLE Event ADD COLUMN IF NOT EXISTS invitation_state TEXT;
UPDATE Event SET provider_kind = 'ExternalUrl' WHERE is_online = TRUE AND provider_kind IS NULL;

ALTER TABLE EventSeries ADD COLUMN IF NOT EXISTS provider_kind TEXT;
ALTER TABLE EventSeries ADD COLUMN IF NOT EXISTS meeting_ref TEXT;
ALTER TABLE EventSeries ADD COLUMN IF NOT EXISTS invitation_state TEXT;
UPDATE EventSeries SET provider_kind = 'ExternalUrl' WHERE is_online = TRUE AND provider_kind IS NULL;
```

Update `kInsertEventQuery`:

```cpp
constexpr auto kInsertEventQuery = R"duckdb(
INSERT INTO Event (
    id,
    name, description, is_work_event,
    event_stat_id, payment_stat_id,
    start_date, end_date, duration, cost,
    is_online, meeting_url, series_id, original_occurrence_start,
    cancellation_reason, canceled_by, buffer_before_minutes, buffer_after_minutes,
    provider_kind, meeting_ref, invitation_state
)
SELECT
    COALESCE(MAX(id), 0) + 1,
    $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18, $19, $20
FROM Event
RETURNING id
)duckdb";
```

Update `kUpdateEventQuery`:

```cpp
constexpr auto kUpdateEventQuery = R"duckdb(
UPDATE Event
SET name = $1,
    description = $2,
    is_work_event = $3,
    event_stat_id = COALESCE($4, event_stat_id),
    payment_stat_id = COALESCE($5, payment_stat_id),
    start_date = $6,
    end_date = $7,
    duration = $8,
    cost = $9,
    is_online = $10,
    meeting_url = $11,
    series_id = $12,
    original_occurrence_start = $13,
    cancellation_reason = $14,
    canceled_by = $15,
    buffer_before_minutes = $16,
    buffer_after_minutes = $17,
    provider_kind = $18,
    meeting_ref = $19,
    invitation_state = $20,
    reminder_notified_at = CASE
        WHEN start_date IS DISTINCT FROM $6 OR end_date IS DISTINCT FROM $7 THEN NULL
        ELSE reminder_notified_at
    END
WHERE id = $21
)duckdb";
```

Update `kInsertEventSeriesQuery`:

```cpp
constexpr auto kInsertEventSeriesQuery = R"duckdb(
INSERT INTO EventSeries (
    id,
    name, description, client_id, is_work_event,
    event_stat_id, payment_stat_id,
    start_date, end_date, duration, cost,
    is_online, meeting_url, recurrence_rule, recurrence_until,
    active, created_at, updated_at, cancellation_reason, canceled_by,
    buffer_before_minutes, buffer_after_minutes,
    provider_kind, meeting_ref, invitation_state
)
SELECT
    COALESCE((SELECT MAX(id) FROM EventSeries), 0) + 1,
    $1, $2, $3, $4, $5, $6, $7, $8, $9, $10,
    $11, $12, $13, $14, TRUE, $15, $15, $16, $17, $18, $19, $20, $21, $22
RETURNING id
)duckdb";
```

Update `kUpdateEventSeriesQuery`:

```cpp
constexpr auto kUpdateEventSeriesQuery = R"duckdb(
UPDATE EventSeries
SET name = $1,
    description = $2,
    client_id = $3,
    is_work_event = $4,
    event_stat_id = $5,
    payment_stat_id = $6,
    start_date = $7,
    end_date = $8,
    duration = $9,
    cost = $10,
    is_online = $11,
    meeting_url = $12,
    recurrence_rule = $13,
    recurrence_until = $14,
    updated_at = $15,
    cancellation_reason = $16,
    canceled_by = $17,
    buffer_before_minutes = $18,
    buffer_after_minutes = $19,
    provider_kind = $20,
    meeting_ref = $21,
    invitation_state = $22
WHERE id = $23
)duckdb";
```

- [ ] **Step 4: Add the fields to `DuckEvent`/`DuckEventSeries`**

In `src/database/schema.hpp`, add three members to `DuckEvent` (after `is_virtual_occurrence`):

```cpp
  bool is_virtual_occurrence = false;
  std::optional<std::string> provider_kind = std::nullopt;
  std::optional<std::string> meeting_ref = std::nullopt;
  std::optional<std::string> invitation_state = std::nullopt;
```

and decode them at the end of `DuckEvent`'s `DataChunk` constructor, following the existing `chunk.ColumnCount() > N` guard style (after the `readBufferMinutes` call, columns 19/20/21 match the three columns appended at the end of `CREATE TABLE Event`):

```cpp
    readBufferMinutes(chunk, index, 17, 18, buffer_before_minutes,
                      buffer_after_minutes);
    if (chunk.ColumnCount() > 19) {
      provider_kind = db_utils::toOptionalString(chunk.GetValue(19, index));
    }
    if (chunk.ColumnCount() > 20) {
      meeting_ref = db_utils::toOptionalString(chunk.GetValue(20, index));
    }
    if (chunk.ColumnCount() > 21) {
      invitation_state = db_utils::toOptionalString(chunk.GetValue(21, index));
    }
  }
```

Add the same three members to `DuckEventSeries` (after `buffer_after_minutes`):

```cpp
  std::int64_t buffer_before_minutes = 0;
  std::int64_t buffer_after_minutes = 0;
  std::optional<std::string> provider_kind = std::nullopt;
  std::optional<std::string> meeting_ref = std::nullopt;
  std::optional<std::string> invitation_state = std::nullopt;
```

and decode them at the end of `DuckEventSeries`'s constructor (columns 20/21/22, matching the three columns appended at the end of `CREATE TABLE EventSeries`):

```cpp
    readBufferMinutes(chunk, index, 18, 19, buffer_before_minutes,
                      buffer_after_minutes);
    if (chunk.ColumnCount() > 20) {
      provider_kind = db_utils::toOptionalString(chunk.GetValue(20, index));
    }
    if (chunk.ColumnCount() > 21) {
      meeting_ref = db_utils::toOptionalString(chunk.GetValue(21, index));
    }
    if (chunk.ColumnCount() > 22) {
      invitation_state = db_utils::toOptionalString(chunk.GetValue(22, index));
    }
  }
```

- [ ] **Step 5: Bind the new fields in `database.cpp`**

In `Database::add_event` (`src/database/database.cpp`), append to the bind list passed to `executePrepared(conn, constance::kInsertEventQuery, {...})` (after `buffer_after_minutes`):

```cpp
       duckdb::Value::INTEGER(static_cast<int32_t>(event.buffer_after_minutes)),
       db_utils::toDuckValue(event.provider_kind),
       db_utils::toDuckValue(event.meeting_ref),
       db_utils::toDuckValue(event.invitation_state)});
```

In `Database::update_event`, append to the `kUpdateEventQuery` bind list the same three values, before the trailing `duckdb::Value::BIGINT(event.id)`:

```cpp
       duckdb::Value::INTEGER(static_cast<int32_t>(event.buffer_after_minutes)),
       db_utils::toDuckValue(event.provider_kind),
       db_utils::toDuckValue(event.meeting_ref),
       db_utils::toDuckValue(event.invitation_state),
       duckdb::Value::BIGINT(event.id)});
```

In `Database::add_event_series`, append to the `kInsertEventSeriesQuery` bind list (after `buffer_after_minutes`):

```cpp
       duckdb::Value::INTEGER(static_cast<int32_t>(series.buffer_after_minutes)),
       db_utils::toDuckValue(series.provider_kind),
       db_utils::toDuckValue(series.meeting_ref),
       db_utils::toDuckValue(series.invitation_state)});
```

In `Database::update_event_series`, append to the `kUpdateEventSeriesQuery` bind list the same three values, before the trailing `duckdb::Value::BIGINT(series.id)`:

```cpp
       duckdb::Value::INTEGER(static_cast<int32_t>(series.buffer_after_minutes)),
       db_utils::toDuckValue(series.provider_kind),
       db_utils::toDuckValue(series.meeting_ref),
       db_utils::toDuckValue(series.invitation_state),
       duckdb::Value::BIGINT(series.id)});
```

- [ ] **Step 6: Run tests to verify they pass**

Run: `cmake --build build --target Sessio_database_tests --parallel && ctest --test-dir build -R DatabaseTest --output-on-failure`
Expected: all `DatabaseTest.*` PASS, including the three new tests.

- [ ] **Step 7: Commit**

```bash
/usr/bin/git add src/database/constants.hpp src/database/schema.hpp src/database/database.cpp test/database_tests.cpp
/usr/bin/git commit -m "feat: persist provider_kind/meeting_ref/invitation_state on Event and EventSeries"
```

---

### Task 2: `ProviderKind`, `MeetingDescriptor`, `MeetingProvider` interface, and `ExternalUrlMeetingProvider`

**Files:**
- Create: `src/meeting/CMakeLists.txt`
- Create: `src/meeting/provider_kind.h`
- Create: `src/meeting/meeting_descriptor.h`
- Create: `src/meeting/meeting_provider.h`
- Create: `src/meeting/external_url_meeting_provider.h`
- Create: `src/meeting/external_url_meeting_provider.cpp`
- Modify: `CMakeLists.txt` (top-level, add `add_subdirectory(src/meeting)`)
- Test: `test/CMakeLists.txt`, `test/provider_kind_tests.cpp`, `test/external_url_meeting_provider_tests.cpp`

**Interfaces:**
- Consumes: nothing from earlier tasks (this task is self-contained; Task 1's DB fields are read/written only by later tasks).
- Produces: `pcm::meeting::ProviderKind` (enum class `ExternalUrl`, `LiveKit`), `pcm::meeting::providerKindToString(ProviderKind) -> std::string`, `pcm::meeting::providerKindFromString(const std::string&) -> std::optional<ProviderKind>`, `pcm::meeting::MeetingDescriptor { ProviderKind kind; QString meetingRef; std::optional<QString> meetingUrl; std::optional<QString> invitationState; }`, `pcm::meeting::MeetingCreateRequest { QString rawMeetingUrl; }`, the abstract `pcm::meeting::MeetingProvider` (signals `created(MeetingDescriptor)`, `createFailed(QString)`, `canceled()`, `cancelFailed(QString)`; virtuals `create(const MeetingCreateRequest&)`, `cancel(const QString&)`), and `pcm::meeting::ExternalUrlMeetingProvider`, a concrete `MeetingProvider`. Task 3 and Task 4 depend on all of these exact names/signatures.

- [ ] **Step 1: Write the failing tests**

Create `test/provider_kind_tests.cpp`:

```cpp
#include "provider_kind.h"

#include <gtest/gtest.h>

TEST(ProviderKindTest, RoundTripsExternalUrl) {
  const auto text = pcm::meeting::providerKindToString(pcm::meeting::ProviderKind::ExternalUrl);
  EXPECT_EQ(text, "ExternalUrl");
  const auto parsed = pcm::meeting::providerKindFromString(text);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(*parsed, pcm::meeting::ProviderKind::ExternalUrl);
}

TEST(ProviderKindTest, RoundTripsLiveKit) {
  const auto text = pcm::meeting::providerKindToString(pcm::meeting::ProviderKind::LiveKit);
  EXPECT_EQ(text, "LiveKit");
  const auto parsed = pcm::meeting::providerKindFromString(text);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(*parsed, pcm::meeting::ProviderKind::LiveKit);
}

TEST(ProviderKindTest, UnknownStringParsesToNullopt) {
  EXPECT_FALSE(pcm::meeting::providerKindFromString("SomethingElse").has_value());
  EXPECT_FALSE(pcm::meeting::providerKindFromString("").has_value());
}
```

Create `test/external_url_meeting_provider_tests.cpp`:

```cpp
#include "external_url_meeting_provider.h"

#include <gtest/gtest.h>

namespace {

class Listener : public QObject {
  Q_OBJECT
public:
  std::optional<pcm::meeting::MeetingDescriptor> lastDescriptor;
  std::optional<QString> lastCreateError;
  bool cancelSucceeded = false;
  std::optional<QString> lastCancelError;

public slots:
  void onCreated(pcm::meeting::MeetingDescriptor descriptor) { lastDescriptor = descriptor; }
  void onCreateFailed(QString error) { lastCreateError = error; }
  void onCanceled() { cancelSucceeded = true; }
  void onCancelFailed(QString error) { lastCancelError = error; }
};

} // namespace

TEST(ExternalUrlMeetingProviderTest, CreateEmitsDescriptorMatchingTheGivenUrl) {
  pcm::meeting::ExternalUrlMeetingProvider provider;
  Listener listener;
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::created, &listener,
                    &Listener::onCreated);
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::createFailed, &listener,
                    &Listener::onCreateFailed);

  provider.create({.rawMeetingUrl = "https://meet.example.invalid/room-1"});

  ASSERT_TRUE(listener.lastDescriptor.has_value());
  EXPECT_FALSE(listener.lastCreateError.has_value());
  EXPECT_EQ(listener.lastDescriptor->kind, pcm::meeting::ProviderKind::ExternalUrl);
  EXPECT_EQ(listener.lastDescriptor->meetingRef, QStringLiteral("https://meet.example.invalid/room-1"));
  ASSERT_TRUE(listener.lastDescriptor->meetingUrl.has_value());
  EXPECT_EQ(*listener.lastDescriptor->meetingUrl, QStringLiteral("https://meet.example.invalid/room-1"));
  EXPECT_FALSE(listener.lastDescriptor->invitationState.has_value());
}

TEST(ExternalUrlMeetingProviderTest, CreateWithEmptyUrlStillEmitsADescriptor) {
  pcm::meeting::ExternalUrlMeetingProvider provider;
  Listener listener;
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::created, &listener,
                    &Listener::onCreated);

  provider.create({.rawMeetingUrl = "   "});

  ASSERT_TRUE(listener.lastDescriptor.has_value());
  EXPECT_EQ(listener.lastDescriptor->kind, pcm::meeting::ProviderKind::ExternalUrl);
  EXPECT_EQ(listener.lastDescriptor->meetingRef, QStringLiteral(""));
  EXPECT_FALSE(listener.lastDescriptor->meetingUrl.has_value());
}

TEST(ExternalUrlMeetingProviderTest, CancelAlwaysSucceeds) {
  pcm::meeting::ExternalUrlMeetingProvider provider;
  Listener listener;
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::canceled, &listener,
                    &Listener::onCanceled);

  provider.cancel("https://meet.example.invalid/room-1");

  EXPECT_TRUE(listener.cancelSucceeded);
}

#include "external_url_meeting_provider_tests.moc"
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target Sessio_provider_kind_tests --parallel`
Expected: compilation failure — `provider_kind.h` does not exist.

- [ ] **Step 3: Implement `provider_kind.h`**

Create `src/meeting/provider_kind.h`:

```cpp
#pragma once

#include <optional>
#include <string>

namespace pcm::meeting {

enum class ProviderKind { ExternalUrl, LiveKit };

[[nodiscard]] inline std::string providerKindToString(const ProviderKind kind) {
  switch (kind) {
  case ProviderKind::ExternalUrl:
    return "ExternalUrl";
  case ProviderKind::LiveKit:
    return "LiveKit";
  }
  return "ExternalUrl";
}

[[nodiscard]] inline std::optional<ProviderKind>
providerKindFromString(const std::string &value) {
  if (value == "ExternalUrl") {
    return ProviderKind::ExternalUrl;
  }
  if (value == "LiveKit") {
    return ProviderKind::LiveKit;
  }
  return std::nullopt;
}

} // namespace pcm::meeting
```

- [ ] **Step 4: Implement `meeting_descriptor.h`**

Create `src/meeting/meeting_descriptor.h`:

```cpp
#pragma once

#include "provider_kind.h"

#include <QMetaType>
#include <QString>
#include <optional>

namespace pcm::meeting {

struct MeetingDescriptor {
  ProviderKind kind = ProviderKind::ExternalUrl;
  QString meetingRef;
  std::optional<QString> meetingUrl;
  std::optional<QString> invitationState;
};

} // namespace pcm::meeting

Q_DECLARE_METATYPE(pcm::meeting::MeetingDescriptor)
```

- [ ] **Step 5: Implement `meeting_provider.h`**

Create `src/meeting/meeting_provider.h`:

```cpp
#pragma once

#include "meeting_descriptor.h"

#include <QObject>
#include <QString>

namespace pcm::meeting {

struct MeetingCreateRequest {
  QString rawMeetingUrl;
};

class MeetingProvider : public QObject {
  Q_OBJECT

public:
  using QObject::QObject;
  ~MeetingProvider() override = default;

  virtual void create(const MeetingCreateRequest &request) = 0;
  virtual void cancel(const QString &meetingRef) = 0;

signals:
  void created(pcm::meeting::MeetingDescriptor descriptor);
  void createFailed(QString error);
  void canceled();
  void cancelFailed(QString error);
};

} // namespace pcm::meeting
```

- [ ] **Step 6: Implement `ExternalUrlMeetingProvider`**

Create `src/meeting/external_url_meeting_provider.h`:

```cpp
#pragma once

#include "meeting_provider.h"

namespace pcm::meeting {

class ExternalUrlMeetingProvider final : public MeetingProvider {
  Q_OBJECT

public:
  using MeetingProvider::MeetingProvider;

  void create(const MeetingCreateRequest &request) override;
  void cancel(const QString &meetingRef) override;
};

} // namespace pcm::meeting
```

Create `src/meeting/external_url_meeting_provider.cpp`:

```cpp
#include "external_url_meeting_provider.h"

namespace pcm::meeting {

void ExternalUrlMeetingProvider::create(const MeetingCreateRequest &request) {
  const auto trimmedUrl = request.rawMeetingUrl.trimmed();

  MeetingDescriptor descriptor;
  descriptor.kind = ProviderKind::ExternalUrl;
  descriptor.meetingRef = trimmedUrl;
  descriptor.meetingUrl = trimmedUrl.isEmpty() ? std::nullopt : std::make_optional(trimmedUrl);
  descriptor.invitationState = std::nullopt;

  emit created(descriptor);
}

void ExternalUrlMeetingProvider::cancel(const QString &meetingRef) {
  Q_UNUSED(meetingRef);
  emit canceled();
}

} // namespace pcm::meeting
```

- [ ] **Step 7: Wire the new library into CMake**

Create `src/meeting/CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME ${PROJECT_NAME}_meeting)

find_package(Qt6 REQUIRED COMPONENTS Core)

qt_add_library(${TARGET_NAME} STATIC
        provider_kind.h
        meeting_descriptor.h
        meeting_provider.h
        external_url_meeting_provider.h
        external_url_meeting_provider.cpp
)

target_link_libraries(${TARGET_NAME} PUBLIC
        Qt6::Core
)

target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
```

In the top-level `CMakeLists.txt`, add the new subdirectory right after `src/database` (before `src/backup`):

```cmake
add_subdirectory(src/database)
add_subdirectory(src/meeting)
add_subdirectory(src/backup)
```

In `test/CMakeLists.txt`, add two new test executables (near the other small, focused test targets):

```cmake
# Provider kind tests
add_executable(Sessio_provider_kind_tests provider_kind_tests.cpp)
target_link_libraries(Sessio_provider_kind_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    Sessio_meeting
)
gtest_discover_tests(Sessio_provider_kind_tests)

# External URL meeting provider tests
add_executable(Sessio_external_url_meeting_provider_tests external_url_meeting_provider_tests.cpp)
target_link_libraries(Sessio_external_url_meeting_provider_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    Sessio_meeting
)
# This test file defines a Q_OBJECT class directly in the .cpp (with a
# matching #include "....moc" at the bottom) and needs moc run on it, the same
# way qcustomplot's target needs it set explicitly elsewhere in this project.
set_target_properties(Sessio_external_url_meeting_provider_tests PROPERTIES AUTOMOC ON)
gtest_discover_tests(Sessio_external_url_meeting_provider_tests)
```

- [ ] **Step 8: Run tests to verify they pass**

Run: `cmake -S . -B build -DPCM_BUILD_TESTS=ON && cmake --build build --parallel && ctest --test-dir build -R "ProviderKindTest|ExternalUrlMeetingProviderTest" --output-on-failure`
Expected: all `ProviderKindTest.*` and `ExternalUrlMeetingProviderTest.*` PASS.

- [ ] **Step 9: Commit**

```bash
/usr/bin/git add src/meeting CMakeLists.txt test/CMakeLists.txt test/provider_kind_tests.cpp test/external_url_meeting_provider_tests.cpp
/usr/bin/git commit -m "feat: add MeetingProvider interface and ExternalUrlMeetingProvider"
```

---

### Task 3: `LiveKitMeetingProvider` stub

**Files:**
- Create: `src/meeting/livekit_meeting_provider.h`
- Create: `src/meeting/livekit_meeting_provider.cpp`
- Modify: `src/meeting/CMakeLists.txt`
- Test: `test/CMakeLists.txt`, `test/livekit_meeting_provider_tests.cpp`

**Interfaces:**
- Consumes: `pcm::meeting::MeetingProvider`, `MeetingCreateRequest` (Task 2).
- Produces: `pcm::meeting::LiveKitMeetingProvider`, a `MeetingProvider` that always reports failure. Task 4's `MeetingCoordinator` owns one instance of this class.

- [ ] **Step 1: Write the failing tests**

Create `test/livekit_meeting_provider_tests.cpp`:

```cpp
#include "livekit_meeting_provider.h"

#include <gtest/gtest.h>

namespace {

class Listener : public QObject {
  Q_OBJECT
public:
  bool createSucceeded = false;
  std::optional<QString> lastCreateError;
  bool cancelSucceeded = false;
  std::optional<QString> lastCancelError;

public slots:
  void onCreated(pcm::meeting::MeetingDescriptor) { createSucceeded = true; }
  void onCreateFailed(QString error) { lastCreateError = error; }
  void onCanceled() { cancelSucceeded = true; }
  void onCancelFailed(QString error) { lastCancelError = error; }
};

} // namespace

TEST(LiveKitMeetingProviderTest, CreateAlwaysFailsWithoutClaimingSuccess) {
  pcm::meeting::LiveKitMeetingProvider provider;
  Listener listener;
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::created, &listener,
                    &Listener::onCreated);
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::createFailed, &listener,
                    &Listener::onCreateFailed);

  provider.create({.rawMeetingUrl = ""});

  EXPECT_FALSE(listener.createSucceeded);
  ASSERT_TRUE(listener.lastCreateError.has_value());
  EXPECT_FALSE(listener.lastCreateError->isEmpty());
}

TEST(LiveKitMeetingProviderTest, CancelAlwaysFailsWithoutClaimingSuccess) {
  pcm::meeting::LiveKitMeetingProvider provider;
  Listener listener;
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::canceled, &listener,
                    &Listener::onCanceled);
  QObject::connect(&provider, &pcm::meeting::MeetingProvider::cancelFailed, &listener,
                    &Listener::onCancelFailed);

  provider.cancel("some-meeting-ref");

  EXPECT_FALSE(listener.cancelSucceeded);
  ASSERT_TRUE(listener.lastCancelError.has_value());
  EXPECT_FALSE(listener.lastCancelError->isEmpty());
}

#include "livekit_meeting_provider_tests.moc"
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target Sessio_livekit_meeting_provider_tests --parallel`
Expected: compilation failure — `livekit_meeting_provider.h` does not exist.

- [ ] **Step 3: Implement the stub**

Create `src/meeting/livekit_meeting_provider.h`:

```cpp
#pragma once

#include "meeting_provider.h"

namespace pcm::meeting {

// Non-functional in this version: satisfies MeetingProvider but never makes a
// network call. LiveKit is not yet selectable anywhere in the UI; the real
// token-backend integration lands with the native call UI.
class LiveKitMeetingProvider final : public MeetingProvider {
  Q_OBJECT

public:
  using MeetingProvider::MeetingProvider;

  void create(const MeetingCreateRequest &request) override;
  void cancel(const QString &meetingRef) override;
};

} // namespace pcm::meeting
```

Create `src/meeting/livekit_meeting_provider.cpp`:

```cpp
#include "livekit_meeting_provider.h"

namespace pcm::meeting {

void LiveKitMeetingProvider::create(const MeetingCreateRequest &request) {
  Q_UNUSED(request);
  emit createFailed(
      QStringLiteral("LiveKit meetings are not yet available in this version."));
}

void LiveKitMeetingProvider::cancel(const QString &meetingRef) {
  Q_UNUSED(meetingRef);
  emit cancelFailed(
      QStringLiteral("LiveKit meetings are not yet available in this version."));
}

} // namespace pcm::meeting
```

- [ ] **Step 4: Wire into CMake**

In `src/meeting/CMakeLists.txt`, add the two new files to the `qt_add_library` call:

```cmake
qt_add_library(${TARGET_NAME} STATIC
        provider_kind.h
        meeting_descriptor.h
        meeting_provider.h
        external_url_meeting_provider.h
        external_url_meeting_provider.cpp
        livekit_meeting_provider.h
        livekit_meeting_provider.cpp
)
```

In `test/CMakeLists.txt`, add:

```cmake
# LiveKit meeting provider stub tests
add_executable(Sessio_livekit_meeting_provider_tests livekit_meeting_provider_tests.cpp)
target_link_libraries(Sessio_livekit_meeting_provider_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    Sessio_meeting
)
set_target_properties(Sessio_livekit_meeting_provider_tests PROPERTIES AUTOMOC ON)
gtest_discover_tests(Sessio_livekit_meeting_provider_tests)
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build --parallel && ctest --test-dir build -R LiveKitMeetingProviderTest --output-on-failure`
Expected: all `LiveKitMeetingProviderTest.*` PASS.

- [ ] **Step 6: Commit**

```bash
/usr/bin/git add src/meeting/livekit_meeting_provider.h src/meeting/livekit_meeting_provider.cpp src/meeting/CMakeLists.txt test/CMakeLists.txt test/livekit_meeting_provider_tests.cpp
/usr/bin/git commit -m "feat: add non-functional LiveKitMeetingProvider stub"
```

---

### Task 4: `MeetingCoordinator`

**Files:**
- Create: `src/meeting/meeting_coordinator.h`
- Create: `src/meeting/meeting_coordinator.cpp`
- Modify: `src/meeting/CMakeLists.txt`
- Test: `test/CMakeLists.txt`, `test/meeting_coordinator_tests.cpp`

**Interfaces:**
- Consumes: `pcm::meeting::ProviderKind`, `MeetingCreateRequest`, `ExternalUrlMeetingProvider`, `LiveKitMeetingProvider` (Tasks 2–3).
- Produces: `pcm::meeting::MeetingCoordinator`, a `QObject` with `createMeeting(ProviderKind, const MeetingCreateRequest&)` / `cancelMeeting(ProviderKind, const QString&)` and signals `meetingCreated(MeetingDescriptor)`, `meetingCreateFailed(QString)`, `meetingCanceled()`, `meetingCancelFailed(QString)`. Task 6 (`QEventDetailsWidget`) and Task 7 (`QTimelineModel`) are the only consumers of this class.

- [ ] **Step 1: Write the failing tests**

Create `test/meeting_coordinator_tests.cpp`:

```cpp
#include "meeting_coordinator.h"

#include <gtest/gtest.h>

namespace {

class Listener : public QObject {
  Q_OBJECT
public:
  std::optional<pcm::meeting::MeetingDescriptor> lastDescriptor;
  std::optional<QString> lastCreateError;
  bool cancelSucceeded = false;
  std::optional<QString> lastCancelError;

public slots:
  void onCreated(pcm::meeting::MeetingDescriptor descriptor) { lastDescriptor = descriptor; }
  void onCreateFailed(QString error) { lastCreateError = error; }
  void onCanceled() { cancelSucceeded = true; }
  void onCancelFailed(QString error) { lastCancelError = error; }
};

} // namespace

TEST(MeetingCoordinatorTest, DispatchesCreateToExternalUrlProvider) {
  pcm::meeting::MeetingCoordinator coordinator;
  Listener listener;
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCreated, &listener,
                    &Listener::onCreated);
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCreateFailed, &listener,
                    &Listener::onCreateFailed);

  coordinator.createMeeting(pcm::meeting::ProviderKind::ExternalUrl,
                            {.rawMeetingUrl = "https://meet.example.invalid/room-9"});

  ASSERT_TRUE(listener.lastDescriptor.has_value());
  EXPECT_FALSE(listener.lastCreateError.has_value());
  EXPECT_EQ(listener.lastDescriptor->kind, pcm::meeting::ProviderKind::ExternalUrl);
  EXPECT_EQ(listener.lastDescriptor->meetingRef, QStringLiteral("https://meet.example.invalid/room-9"));
}

TEST(MeetingCoordinatorTest, DispatchesCreateToLiveKitProviderWhichFails) {
  pcm::meeting::MeetingCoordinator coordinator;
  Listener listener;
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCreated, &listener,
                    &Listener::onCreated);
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCreateFailed, &listener,
                    &Listener::onCreateFailed);

  coordinator.createMeeting(pcm::meeting::ProviderKind::LiveKit, {.rawMeetingUrl = ""});

  EXPECT_FALSE(listener.lastDescriptor.has_value());
  ASSERT_TRUE(listener.lastCreateError.has_value());
  EXPECT_FALSE(listener.lastCreateError->isEmpty());
}

TEST(MeetingCoordinatorTest, DispatchesCancelToExternalUrlProvider) {
  pcm::meeting::MeetingCoordinator coordinator;
  Listener listener;
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCanceled, &listener,
                    &Listener::onCanceled);

  coordinator.cancelMeeting(pcm::meeting::ProviderKind::ExternalUrl,
                            "https://meet.example.invalid/room-9");

  EXPECT_TRUE(listener.cancelSucceeded);
}

TEST(MeetingCoordinatorTest, DispatchesCancelToLiveKitProviderWhichFails) {
  pcm::meeting::MeetingCoordinator coordinator;
  Listener listener;
  QObject::connect(&coordinator, &pcm::meeting::MeetingCoordinator::meetingCancelFailed, &listener,
                    &Listener::onCancelFailed);

  coordinator.cancelMeeting(pcm::meeting::ProviderKind::LiveKit, "some-ref");

  ASSERT_TRUE(listener.lastCancelError.has_value());
  EXPECT_FALSE(listener.lastCancelError->isEmpty());
}

#include "meeting_coordinator_tests.moc"
```

- [ ] **Step 2: Run tests to verify they fail**

Run: `cmake --build build --target Sessio_meeting_coordinator_tests --parallel`
Expected: compilation failure — `meeting_coordinator.h` does not exist.

- [ ] **Step 3: Implement `MeetingCoordinator`**

Create `src/meeting/meeting_coordinator.h`:

```cpp
#pragma once

#include "external_url_meeting_provider.h"
#include "livekit_meeting_provider.h"
#include "meeting_provider.h"

namespace pcm::meeting {

// Selects the right MeetingProvider by ProviderKind and drives it on behalf of
// the event-editing UI and the timeline model. This is the only class either
// of those touches directly — neither has to know ExternalUrlMeetingProvider
// or LiveKitMeetingProvider exist.
class MeetingCoordinator final : public QObject {
  Q_OBJECT

public:
  explicit MeetingCoordinator(QObject *parent = nullptr);

  void createMeeting(ProviderKind kind, const MeetingCreateRequest &request);
  void cancelMeeting(ProviderKind kind, const QString &meetingRef);

signals:
  void meetingCreated(pcm::meeting::MeetingDescriptor descriptor);
  void meetingCreateFailed(QString error);
  void meetingCanceled();
  void meetingCancelFailed(QString error);

private:
  [[nodiscard]] MeetingProvider *providerFor(ProviderKind kind) const;

  ExternalUrlMeetingProvider *mExternalUrlProvider;
  LiveKitMeetingProvider *mLiveKitProvider;
};

} // namespace pcm::meeting
```

Create `src/meeting/meeting_coordinator.cpp`:

```cpp
#include "meeting_coordinator.h"

namespace pcm::meeting {

MeetingCoordinator::MeetingCoordinator(QObject *parent)
    : QObject(parent), mExternalUrlProvider(new ExternalUrlMeetingProvider(this)),
      mLiveKitProvider(new LiveKitMeetingProvider(this)) {
  for (auto *provider : {static_cast<MeetingProvider *>(mExternalUrlProvider),
                         static_cast<MeetingProvider *>(mLiveKitProvider)}) {
    connect(provider, &MeetingProvider::created, this, &MeetingCoordinator::meetingCreated);
    connect(provider, &MeetingProvider::createFailed, this,
            &MeetingCoordinator::meetingCreateFailed);
    connect(provider, &MeetingProvider::canceled, this, &MeetingCoordinator::meetingCanceled);
    connect(provider, &MeetingProvider::cancelFailed, this,
            &MeetingCoordinator::meetingCancelFailed);
  }
}

MeetingProvider *MeetingCoordinator::providerFor(const ProviderKind kind) const {
  switch (kind) {
  case ProviderKind::ExternalUrl:
    return mExternalUrlProvider;
  case ProviderKind::LiveKit:
    return mLiveKitProvider;
  }
  return mExternalUrlProvider;
}

void MeetingCoordinator::createMeeting(const ProviderKind kind, const MeetingCreateRequest &request) {
  providerFor(kind)->create(request);
}

void MeetingCoordinator::cancelMeeting(const ProviderKind kind, const QString &meetingRef) {
  providerFor(kind)->cancel(meetingRef);
}

} // namespace pcm::meeting
```

- [ ] **Step 4: Wire into CMake**

In `src/meeting/CMakeLists.txt`, add to `qt_add_library`:

```cmake
qt_add_library(${TARGET_NAME} STATIC
        provider_kind.h
        meeting_descriptor.h
        meeting_provider.h
        external_url_meeting_provider.h
        external_url_meeting_provider.cpp
        livekit_meeting_provider.h
        livekit_meeting_provider.cpp
        meeting_coordinator.h
        meeting_coordinator.cpp
)
```

In `test/CMakeLists.txt`, add:

```cmake
# Meeting coordinator tests
add_executable(Sessio_meeting_coordinator_tests meeting_coordinator_tests.cpp)
target_link_libraries(Sessio_meeting_coordinator_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    Sessio_meeting
)
set_target_properties(Sessio_meeting_coordinator_tests PROPERTIES AUTOMOC ON)
gtest_discover_tests(Sessio_meeting_coordinator_tests)
```

- [ ] **Step 5: Run tests to verify they pass**

Run: `cmake --build build --parallel && ctest --test-dir build -R MeetingCoordinatorTest --output-on-failure`
Expected: all `MeetingCoordinatorTest.*` PASS.

- [ ] **Step 6: Commit**

```bash
/usr/bin/git add src/meeting/meeting_coordinator.h src/meeting/meeting_coordinator.cpp src/meeting/CMakeLists.txt test/CMakeLists.txt test/meeting_coordinator_tests.cpp
/usr/bin/git commit -m "feat: add MeetingCoordinator to dispatch between meeting providers"
```

---

### Task 5: `QEventItem` provider fields and `EventSeries` propagation

**Files:**
- Modify: `src/event_view/event_item.h`
- Modify: `src/event_view/event_item.cpp`
- Modify: `src/event_view/recurrence_utils.cpp` (`buildVirtualOccurrence`)
- Modify: `src/event_view/qtimeline_model.cpp` (`addEventSeries`, `updateEventSeries`)
- Modify: `src/event_view/CMakeLists.txt` (link `Sessio_meeting`)
- Test: `test/recurrence_utils_tests.cpp`

**Interfaces:**
- Consumes: `DuckEvent::provider_kind/meeting_ref/invitation_state` (Task 1), `pcm::meeting::ProviderKind`, `providerKindToString`/`providerKindFromString` (Task 2).
- Produces: `QEventItem::providerKind() -> std::optional<pcm::meeting::ProviderKind>`, `QEventItem::meetingRef() -> QString`, `QEventItem::invitationState() -> std::optional<QString>`, and matching setters. Task 6 is the sole consumer of these accessors/setters.

- [ ] **Step 1: Write the failing test**

`QEventItem` itself has no existing test file (it is exercised indirectly through `recurrence_utils_tests.cpp`, which already builds `DuckEvent`/`DuckEventSeries` values). Add a test there that exercises the series→occurrence propagation this task adds. Open `test/recurrence_utils_tests.cpp`, find the test(s) covering `buildVirtualOccurrence` (search for that function name), and add, alongside them:

```cpp
TEST(RecurrenceUtilsTest, BuildVirtualOccurrenceCopiesProviderFieldsFromSeries) {
  DuckEventSeries series;
  series.name = std::string{"Weekly Session"};
  series.start_date = 1730000000000;
  series.duration = 3600;
  series.provider_kind = std::string{"ExternalUrl"};
  series.meeting_ref = std::string{"https://meet.example.invalid/room-3"};
  series.invitation_state = std::nullopt;

  const auto occurrenceStart =
      QDateTime::fromMSecsSinceEpoch(*series.start_date, QTimeZone::UTC);
  const auto occurrence = pcm::recurrence::buildVirtualOccurrence(series, occurrenceStart, -1);

  ASSERT_TRUE(occurrence.provider_kind.has_value());
  EXPECT_EQ(*occurrence.provider_kind, "ExternalUrl");
  ASSERT_TRUE(occurrence.meeting_ref.has_value());
  EXPECT_EQ(*occurrence.meeting_ref, "https://meet.example.invalid/room-3");
  EXPECT_FALSE(occurrence.invitation_state.has_value());
}
```

(Match the exact namespace/include style already used at the top of that file for `DuckEventSeries`/`QDateTime`/`QTimeZone` — copy from the neighboring test rather than guessing.)

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target Sessio_recurrence_tests --parallel`
Expected: compilation succeeds (fields already exist from Task 1) but the test FAILS — `occurrence.provider_kind` is `std::nullopt` because `buildVirtualOccurrence` does not copy it yet.

- [ ] **Step 3: Propagate the fields in `recurrence_utils.cpp`**

In `src/event_view/recurrence_utils.cpp`, in `buildVirtualOccurrence` (`src/event_view/recurrence_utils.cpp:142-169`), after the existing `event.meeting_url = series.meeting_url;` line, add:

```cpp
  event.provider_kind = series.provider_kind;
  event.meeting_ref = series.meeting_ref;
  event.invitation_state = series.invitation_state;
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target Sessio_recurrence_tests --parallel && ctest --test-dir build -R RecurrenceUtilsTest --output-on-failure`
Expected: `RecurrenceUtilsTest.BuildVirtualOccurrenceCopiesProviderFieldsFromSeries` PASSES, all other `RecurrenceUtilsTest.*` still PASS.

- [ ] **Step 5: Propagate the fields in `qtimeline_model.cpp`**

In `src/event_view/qtimeline_model.cpp`, in `QTimelineModel::addEventSeries` (`src/event_view/qtimeline_model.cpp:153-177`), after the existing `series.meeting_url = event.meeting_url;` line, add:

```cpp
  series.provider_kind = event.provider_kind;
  series.meeting_ref = event.meeting_ref;
  series.invitation_state = event.invitation_state;
```

In `QTimelineModel::updateEventSeries` (`src/event_view/qtimeline_model.cpp:179-`), after the existing `series.meeting_url = event.meeting_url;` line (around line 205), add the same three lines:

```cpp
  series.provider_kind = event.provider_kind;
  series.meeting_ref = event.meeting_ref;
  series.invitation_state = event.invitation_state;
```

(`QTimelineModel::addEvent`/`updateEvent` need no change — they pass the whole `DuckEvent` straight to `Database::add_event`/`update_event`, which Task 1 already extended.)

- [ ] **Step 6: Add the fields to `QEventItem`**

In `src/event_view/event_item.h`, add `#include "provider_kind.h"` near the top (alongside `"constants.hpp"`/`"schema.hpp"`), then add three accessors after `meetingUrl()`:

```cpp
  [[nodiscard]] bool isOnline() const;
  [[nodiscard]] QString meetingUrl() const;
  [[nodiscard]] std::optional<pcm::meeting::ProviderKind> providerKind() const;
  [[nodiscard]] QString meetingRef() const;
  [[nodiscard]] std::optional<QString> invitationState() const;
```

and three setters after `setMeetingUrl(...)`:

```cpp
  void setOnline(bool online);
  void setMeetingUrl(const QString &meetingUrl);
  void setProviderKind(std::optional<pcm::meeting::ProviderKind> kind);
  void setMeetingRef(const QString &meetingRef);
  void setInvitationState(std::optional<QString> state);
```

and three members after `mMeetingUrl`:

```cpp
  QString mMeetingUrl;
  std::optional<pcm::meeting::ProviderKind> mProviderKind;
  QString mMeetingRef;
  std::optional<QString> mInvitationState;
```

- [ ] **Step 7: Wire the fields through `event_item.cpp`**

In `src/event_view/event_item.cpp`, in `QEventItem::updateFromEvent` (line 90) and the `QEventItem(const DuckEvent&)` constructor (line 128), after each existing `mMeetingUrl = QString::fromStdString(event.meeting_url);` line, add:

```cpp
  mProviderKind = event.provider_kind.has_value()
                      ? pcm::meeting::providerKindFromString(*event.provider_kind)
                      : std::nullopt;
  mMeetingRef = QString::fromStdString(event.meeting_ref.value_or(""));
  mInvitationState = event.invitation_state.has_value()
                          ? std::make_optional(QString::fromStdString(*event.invitation_state))
                          : std::nullopt;
```

In `QEventItem::toEvent()` (line 164), after the existing `event.meeting_url = mMeetingUrl.trimmed().toStdString();` line, add:

```cpp
  event.provider_kind = mProviderKind.has_value()
                             ? std::make_optional(pcm::meeting::providerKindToString(*mProviderKind))
                             : std::nullopt;
  event.meeting_ref =
      mMeetingRef.trimmed().isEmpty() ? std::nullopt : std::make_optional(mMeetingRef.trimmed().toStdString());
  event.invitation_state = mInvitationState.has_value()
                                ? std::make_optional(mInvitationState->toStdString())
                                : std::nullopt;
```

Add the accessor/setter bodies near the existing `isOnline()`/`meetingUrl()`/`setOnline()`/`setMeetingUrl()` definitions (lines 224-225, 286-307):

```cpp
std::optional<pcm::meeting::ProviderKind> QEventItem::providerKind() const { return mProviderKind; }
QString QEventItem::meetingRef() const { return mMeetingRef; }
std::optional<QString> QEventItem::invitationState() const { return mInvitationState; }

void QEventItem::setProviderKind(std::optional<pcm::meeting::ProviderKind> kind) {
  if (mProviderKind == kind)
    return;
  mProviderKind = kind;
  update();
}

void QEventItem::setMeetingRef(const QString &meetingRef) {
  const auto normalizedRef = meetingRef.trimmed();
  if (mMeetingRef == normalizedRef)
    return;
  mMeetingRef = normalizedRef;
  update();
}

void QEventItem::setInvitationState(std::optional<QString> state) {
  if (mInvitationState == state)
    return;
  mInvitationState = std::move(state);
  update();
}
```

- [ ] **Step 8: Link `Sessio_meeting` into `Sessio_event_view`**

In `src/event_view/CMakeLists.txt`, add `${PROJECT_NAME}_meeting` to both `add_dependencies` and `target_link_libraries`:

```cmake
add_dependencies(${TARGET_NAME}
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_widgets
        ${PROJECT_NAME}_meeting
)

target_link_libraries(${TARGET_NAME} PUBLIC
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_widgets
        ${PROJECT_NAME}_meeting
        ical
        Qt6::Widgets
        Qt6::Gui
)
```

- [ ] **Step 9: Run all affected tests to verify they pass**

Run: `cmake --build build --parallel && ctest --test-dir build -R "RecurrenceUtilsTest|DatabaseTest" --output-on-failure`
Expected: all pass, including Task 1's tests (unaffected) and Task 5's new test.

- [ ] **Step 10: Commit**

```bash
/usr/bin/git add src/event_view/event_item.h src/event_view/event_item.cpp src/event_view/recurrence_utils.cpp src/event_view/qtimeline_model.cpp src/event_view/CMakeLists.txt test/recurrence_utils_tests.cpp
/usr/bin/git commit -m "feat: mirror provider fields onto QEventItem and EventSeries occurrences"
```

---

### Task 6: `QEventDetailsWidget` integration with `MeetingCoordinator`

**Files:**
- Modify: `src/pages/event_info_page/qevent_details_widget.h`
- Modify: `src/pages/event_info_page/qevent_details_widget.cpp`
- Modify: `src/pages/event_info_page/CMakeLists.txt` (link `Sessio_meeting`)

**Interfaces:**
- Consumes: `pcm::meeting::MeetingCoordinator`, `ProviderKind`, `MeetingCreateRequest`, `MeetingDescriptor` (Tasks 2, 4); `QEventItem::setProviderKind/setMeetingRef/setInvitationState` (Task 5).
- Produces: `QEventDetailsWidget::setMeetingCoordinator(pcm::meeting::MeetingCoordinator*)`. Task 7 is the consumer (it constructs the coordinator and must call this setter on every `QEventDetailsWidget` it creates).

No new automated tests: this codebase has no widget-level (`QTest`) test infrastructure — every existing `QEventDetailsWidget` behavior is verified manually via `scripts/run-dev-isolated.sh` (see Task 7's manual verification step, which exercises this task's change end-to-end). `MeetingCoordinator`'s own dispatch logic is already covered by Task 4's tests.

- [ ] **Step 1: Add the setter and a dedicated slot**

`MeetingCoordinator` is a long-lived singleton (owned by `Application`, Task 7) while `QEventDetailsWidget` instances are created and destroyed per-dialog, and the same widget's `onApplyClicked` can fire more than once (the widget supports an Apply button, not just a one-shot Save). Connecting a fresh lambda to `mMeetingCoordinator`'s signal on every save would accumulate one connection per click. Avoid that by connecting exactly once, to a real slot, inside `setMeetingCoordinator` itself.

In `src/pages/event_info_page/qevent_details_widget.h`, add `#include "meeting_coordinator.h"` and declare the setter alongside `setConflictChecker`:

```cpp
  void setConflictChecker(
      std::function<std::optional<DuckEvent>(const DuckEvent &)> checker);
  void setMeetingCoordinator(pcm::meeting::MeetingCoordinator *coordinator);
```

Add a private slot declaration alongside the existing `onMeetingUrlChanged`:

```cpp
  void onMeetingUrlChanged(const QString &url);
  void onMeetingCreated(pcm::meeting::MeetingDescriptor descriptor);
```

Add the member alongside `mConflictChecker`:

```cpp
  std::function<std::optional<DuckEvent>(const DuckEvent &)> mConflictChecker;
  QPointer<pcm::meeting::MeetingCoordinator> mMeetingCoordinator;
```

- [ ] **Step 2: Implement the setter and the slot**

In `src/pages/event_info_page/qevent_details_widget.cpp`, add near `setConflictChecker`'s existing implementation:

```cpp
void QEventDetailsWidget::setMeetingCoordinator(pcm::meeting::MeetingCoordinator *coordinator) {
  if (mMeetingCoordinator) {
    disconnect(mMeetingCoordinator, &pcm::meeting::MeetingCoordinator::meetingCreated, this,
              &QEventDetailsWidget::onMeetingCreated);
  }
  mMeetingCoordinator = coordinator;
  if (mMeetingCoordinator) {
    connect(mMeetingCoordinator, &pcm::meeting::MeetingCoordinator::meetingCreated, this,
            &QEventDetailsWidget::onMeetingCreated);
  }
}

void QEventDetailsWidget::onMeetingCreated(const pcm::meeting::MeetingDescriptor descriptor) {
  if (!mCurrentEvent) {
    return;
  }
  mCurrentEvent->setProviderKind(descriptor.kind);
  mCurrentEvent->setMeetingRef(descriptor.meetingRef);
  mCurrentEvent->setInvitationState(descriptor.invitationState);
  mCurrentEvent->setMeetingUrl(descriptor.meetingUrl.value_or(QString{}));
}
```

- [ ] **Step 3: Restructure the save flow in `onApplyClicked`**

This is the core of the task. `onApplyClicked` (`src/pages/event_info_page/qevent_details_widget.cpp:687-`) currently sets `is_online`/`meeting_url` directly (lines 726-729) and then emits `provideEventSave` synchronously. Replace those two lines with a call into `MeetingCoordinator` that — for `ExternalUrl`, whose `create`/`cancel` both emit synchronously (Task 2) — completes before `onApplyClicked` continues, so the rest of the function does not need to change shape.

Replace:

```cpp
    mCurrentEvent->setOnline(mOnlineSessionSwitch->isChecked());
    mCurrentEvent->setMeetingUrl(mOnlineSessionSwitch->isChecked()
                                     ? mMeetingUrlEdit->text()
                                     : QString{});
```

with:

```cpp
    mCurrentEvent->setOnline(mOnlineSessionSwitch->isChecked());
    updateMeetingViaCoordinator();
```

Add a new private method `updateMeetingViaCoordinator()`. Declare it in `qevent_details_widget.h` next to `validateInput`/`collectEventData`:

```cpp
  // --- Validation & Data Collection ---
  bool validateInput();
  [[nodiscard]] DuckEvent collectEventData() const;
  void updateMeetingViaCoordinator();
```

Implement it in `qevent_details_widget.cpp`, near `validateInput`:

```cpp
void QEventDetailsWidget::updateMeetingViaCoordinator() {
  if (!mCurrentEvent) {
    return;
  }

  const bool wasOnline = mCurrentEvent->providerKind().has_value();
  const bool isOnline = mOnlineSessionSwitch->isChecked();

  if (!isOnline) {
    if (wasOnline && mMeetingCoordinator) {
      // Fire-and-forget: ExternalUrl/LiveKit cancel() are both no-ops/errors
      // that carry no state the UI needs to react to synchronously.
      mMeetingCoordinator->cancelMeeting(*mCurrentEvent->providerKind(),
                                         mCurrentEvent->meetingRef());
    }
    mCurrentEvent->setProviderKind(std::nullopt);
    mCurrentEvent->setMeetingRef(QString{});
    mCurrentEvent->setInvitationState(std::nullopt);
    mCurrentEvent->setMeetingUrl(QString{});
    return;
  }

  if (!mMeetingCoordinator) {
    // Defensive fallback (should not happen once Task 7 wires the coordinator
    // everywhere QEventDetailsWidget is constructed): preserve today's
    // behavior instead of silently dropping the link.
    mCurrentEvent->setProviderKind(pcm::meeting::ProviderKind::ExternalUrl);
    mCurrentEvent->setMeetingRef(mMeetingUrlEdit->text().trimmed());
    mCurrentEvent->setMeetingUrl(mMeetingUrlEdit->text().trimmed());
    return;
  }

  // ExternalUrlMeetingProvider::create emits `created` synchronously (Task 2),
  // and onMeetingCreated (connected once, in setMeetingCoordinator) applies the
  // descriptor to mCurrentEvent before this call returns.
  mMeetingCoordinator->createMeeting(pcm::meeting::ProviderKind::ExternalUrl,
                                     {.rawMeetingUrl = mMeetingUrlEdit->text()});
}
```

- [ ] **Step 4: Link `Sessio_meeting` into `Sessio_event_page`**

In `src/pages/event_info_page/CMakeLists.txt`, add `${PROJECT_NAME}_meeting` to both `add_dependencies` and `target_link_libraries`:

```cmake
add_dependencies(${TARGET_NAME}
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_client_model
        ${PROJECT_NAME}_widgets
        ${PROJECT_NAME}_event_view
        ${PROJECT_NAME}_timeline_widget
        ${PROJECT_NAME}_meeting
)

target_link_libraries(${TARGET_NAME} PUBLIC
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_client_model
        ${PROJECT_NAME}_widgets
        ${PROJECT_NAME}_event_view
        ${PROJECT_NAME}_timeline_widget
        ${PROJECT_NAME}_meeting
        qlementine
        Qt6::Widgets
        Qt6::Gui
)
```

- [ ] **Step 5: Build to verify no compile errors**

Run: `cmake --build build --parallel`
Expected: the `Sessio_event_page` target and the full app build cleanly. (This task has no automated test of its own — Task 7's manual verification step is what exercises this code path end-to-end.)

- [ ] **Step 6: Commit**

```bash
/usr/bin/git add src/pages/event_info_page/qevent_details_widget.h src/pages/event_info_page/qevent_details_widget.cpp src/pages/event_info_page/CMakeLists.txt
/usr/bin/git commit -m "feat: route QEventDetailsWidget's meeting field through MeetingCoordinator"
```

---

### Task 7: Dependency wiring and cancel-on-delete

**Files:**
- Modify: `src/app/application.cpp`
- Modify: `src/app/main_window.h`
- Modify: `src/app/main_window.cpp`
- Modify: `src/pages/event_info_page/event_info.h`
- Modify: `src/pages/event_info_page/event_info.cpp`
- Modify: `src/event_view/qtimeline_model.h`
- Modify: `src/event_view/qtimeline_model.cpp` (`removeEvent`)
- Modify: `src/app/CMakeLists.txt` (link `Sessio_meeting`)

**Interfaces:**
- Consumes: `pcm::meeting::MeetingCoordinator` (Task 4), `QEventDetailsWidget::setMeetingCoordinator` (Task 6).
- Produces: one `MeetingCoordinator` instance, owned by `Application`, threaded down to every `QEventDetailsWidget` and into `QTimelineModel`.

No new automated test: this task is DI plumbing plus a change to a deletion code path with no existing test coverage (`QTimelineModel::removeEvent` has none today — deletion is exercised only through `Sessio_database_tests`' `RemoveEventSucceedsAfterChangeLogRowsExist`, which tests `Database::remove_event` directly, not `QTimelineModel`). Verification is manual (Step 6).

- [ ] **Step 1: Construct `MeetingCoordinator` once, in `Application`**

In `src/app/application.cpp`, near where `mDb`/`mAutoBackupScheduler` are constructed (before `mMainWindow->addEventInfoPage(...)`), add:

```cpp
  mMeetingCoordinator = std::make_unique<pcm::meeting::MeetingCoordinator>(this);
```

In `src/app/application.h`, add `#include "meeting_coordinator.h"` next to the existing `#include "event_info.h"` (line 19), and add a member next to `mAutoBackupScheduler` (line 65):

```cpp
  std::unique_ptr<pcm::backup::AutoBackupScheduler> mAutoBackupScheduler;
  std::unique_ptr<pcm::meeting::MeetingCoordinator> mMeetingCoordinator;
```

Change the `QTimelineModel` construction line:

```cpp
  mMainWindow->addEventInfoPage(new QTimelineModel(mDb, this));
```

to:

```cpp
  mMainWindow->addEventInfoPage(new QTimelineModel(mDb, mMeetingCoordinator.get(), this));
```

- [ ] **Step 2: Thread the coordinator into `QTimelineModel`**

In `src/event_view/qtimeline_model.h`, add the parameter and a member:

```cpp
  explicit QTimelineModel(const std::shared_ptr<pcm::database::Database> &db,
                          pcm::meeting::MeetingCoordinator *meetingCoordinator,
                          QObject *parent = nullptr);
```

```cpp
  QPointer<pcm::meeting::MeetingCoordinator> mMeetingCoordinator;
```

Add `#include "meeting_coordinator.h"` and `#include <QPointer>` near the top.

In `src/event_view/qtimeline_model.cpp`, replace the constructor:

```cpp
QTimelineModel::QTimelineModel(
    const std::shared_ptr<pcm::database::Database> &db, QObject *parent)
    : QAbstractItemModel(parent), mDb(db) {}
```

with:

```cpp
QTimelineModel::QTimelineModel(
    const std::shared_ptr<pcm::database::Database> &db,
    pcm::meeting::MeetingCoordinator *meetingCoordinator, QObject *parent)
    : QAbstractItemModel(parent), mDb(db), mMeetingCoordinator(meetingCoordinator) {}
```

- [ ] **Step 3: Call `cancelMeeting` before deleting an event**

In `QTimelineModel::removeEvent` (`src/event_view/qtimeline_model.cpp:266-298`), the function has two call sites of `mDb->remove_event(id)` — one for a materialized recurring override, one for a plain event. Before EACH `mDb->remove_event(id)` call, insert:

```cpp
      if (mEvents[i].provider_kind.has_value() && mMeetingCoordinator) {
        const auto kind = pcm::meeting::providerKindFromString(*mEvents[i].provider_kind);
        if (kind.has_value()) {
          mMeetingCoordinator->cancelMeeting(*kind, QString::fromStdString(
                                                        mEvents[i].meeting_ref.value_or("")));
        }
      }
```

So the function becomes:

```cpp
void QTimelineModel::removeEvent(int64_t id) {
  for (int i = 0; i < mEvents.size(); ++i) {
    if (mEvents[i].id == id) {
      if (mEvents[i].series_id.has_value() &&
          mEvents[i].original_occurrence_start.has_value()) {
        if (!mDb->add_event_series_exception(*mEvents[i].series_id,
                                             *mEvents[i].original_occurrence_start,
                                             "deleted")) {
          qWarning() << "QTimelineModel::removeEvent failed to add series exception for id="
                     << id;
          return;
        }
        if (mEvents[i].provider_kind.has_value() && mMeetingCoordinator) {
          const auto kind = pcm::meeting::providerKindFromString(*mEvents[i].provider_kind);
          if (kind.has_value()) {
            mMeetingCoordinator->cancelMeeting(
                *kind, QString::fromStdString(mEvents[i].meeting_ref.value_or("")));
          }
        }
        if (!mEvents[i].is_virtual_occurrence && !mDb->remove_event(id)) {
          qWarning() << "QTimelineModel::removeEvent failed for recurring override id="
                     << id;
          return;
        }
        beginRemoveRows({}, i, i);
        mEvents.removeAt(i);
        endRemoveRows();
        break;
      }
      if (mEvents[i].provider_kind.has_value() && mMeetingCoordinator) {
        const auto kind = pcm::meeting::providerKindFromString(*mEvents[i].provider_kind);
        if (kind.has_value()) {
          mMeetingCoordinator->cancelMeeting(
              *kind, QString::fromStdString(mEvents[i].meeting_ref.value_or("")));
        }
      }
      if (!mDb->remove_event(id)) {
        qWarning() << "QTimelineModel::removeEvent failed for id=" << id;
        return;
      }
      beginRemoveRows({}, i, i);
      mEvents.removeAt(i);
      endRemoveRows();
      break;
    }
  }
}
```

- [ ] **Step 4: Thread the coordinator down to `QEventInfoPage` and `QEventDetailsWidget`**

In `src/pages/event_info_page/event_info.h`, add `#include "meeting_coordinator.h"` next to the existing `#include "timeline_widget.h"` (line 6), change the constructor declaration (line 27):

```cpp
  QEventInfoPage(QTimelineModel *model, QWidget *parent);
```

to:

```cpp
  QEventInfoPage(QTimelineModel *model, pcm::meeting::MeetingCoordinator *meetingCoordinator,
                QWidget *parent);
```

and add a member next to `mTimelineWidget` (line 66):

```cpp
  QTimelineWidget *mTimelineWidget = nullptr;
  QPointer<pcm::meeting::MeetingCoordinator> mMeetingCoordinator;
```

In `src/pages/event_info_page/event_info.cpp`, change the constructor definition (line 79):

```cpp
QEventInfoPage::QEventInfoPage(QTimelineModel *model, QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::EventInfo>()) {
```

to:

```cpp
QEventInfoPage::QEventInfoPage(QTimelineModel *model,
                               pcm::meeting::MeetingCoordinator *meetingCoordinator,
                               QWidget *parent)
    : QWidget(parent), mUi(std::make_unique<Ui::EventInfo>()),
      mMeetingCoordinator(meetingCoordinator) {
```

Then, in both `openQuickEventDialog` and `openEventDialog` (`src/pages/event_info_page/event_info.cpp:184-220` and `:222-273`), right after each `auto *detailsWidget = new QEventDetailsWidget(&dialog);` line, add:

```cpp
  detailsWidget->setMeetingCoordinator(mMeetingCoordinator);
```

In `src/app/main_window.h`, change the declaration (line 67):

```cpp
  void addEventInfoPage(QTimelineModel *model);
```

to:

```cpp
  void addEventInfoPage(QTimelineModel *model, pcm::meeting::MeetingCoordinator *meetingCoordinator);
```

(add `#include "meeting_coordinator.h"` near `main_window.h`'s other includes).

In `src/app/main_window.cpp`, change:

```cpp
void MainWindow::addEventInfoPage(QTimelineModel *model) {
  const auto page = new QEventInfoPage(model, this);
```

to:

```cpp
void MainWindow::addEventInfoPage(QTimelineModel *model,
                                  pcm::meeting::MeetingCoordinator *meetingCoordinator) {
  const auto page = new QEventInfoPage(model, meetingCoordinator, this);
```

Finally, in `src/app/application.cpp`, update the call from Step 1 to also pass the coordinator to `addEventInfoPage`:

```cpp
  mMainWindow->addEventInfoPage(new QTimelineModel(mDb, mMeetingCoordinator.get(), this),
                                mMeetingCoordinator.get());
```

- [ ] **Step 5: Link `Sessio_meeting` into `Sessio_app`**

In `src/app/CMakeLists.txt`, add `${PROJECT_NAME}_meeting` to `add_dependencies` and `target_link_libraries` (alongside the existing `${PROJECT_NAME}_event_page` entry).

- [ ] **Step 6: Build and manually verify**

Run: `cmake --build build-release --parallel` (release tree, since this step verifies the real app, not the test tree).

Launch the isolated dev build (per this project's established convention — never launch a raw build against real app data):

```bash
scripts/run-dev-isolated.sh
```

Walk through:
1. Create a new event, enable "Online session", type a link, save. Reopen it — the link is still there (unchanged from today's behavior).
2. Turn "Online session" off on an existing online event, save. Reopen it — the link field is empty (unchanged from today's behavior).
3. Create a recurring online event (weekly), save, and open a couple of future occurrences — each carries the same link (unchanged from today's behavior).
4. Delete an online event. No crash, no error dialog (the `MeetingCoordinator::cancelMeeting` call for `ExternalUrl` is a silent no-op).

Expected: the UI behaves identically to before this plan in every one of these flows — this plan's entire visible surface area is "nothing changed," by design (Q3).

- [ ] **Step 7: Commit**

```bash
/usr/bin/git add src/app/application.cpp src/app/application.h src/app/main_window.h src/app/main_window.cpp src/app/CMakeLists.txt src/pages/event_info_page/event_info.h src/pages/event_info_page/event_info.cpp src/event_view/qtimeline_model.h src/event_view/qtimeline_model.cpp
/usr/bin/git commit -m "feat: wire MeetingCoordinator through the app and cancel meetings on delete"
```

---

### Task 8: Version, changelog, and translation check

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/app/application.cpp`
- Modify: `CHANGELOG.md`

**Interfaces:**
- Consumes: nothing (bookkeeping only).
- Produces: nothing further tasks depend on. This is the final task.

- [ ] **Step 1: Bump the version**

In `CMakeLists.txt`, change:

```cmake
project(Sessio VERSION 0.1.32 LANGUAGES CXX)
```

to:

```cmake
project(Sessio VERSION 0.1.33 LANGUAGES CXX)
```

In `src/app/application.cpp`, change:

```cpp
  app.setApplicationVersion("0.1.32");
```

to:

```cpp
  app.setApplicationVersion("0.1.33");
```

- [ ] **Step 2: Add a `CHANGELOG.md` entry**

At the top of `CHANGELOG.md`, before the existing `## [0.1.32] - 2026-09-24` entry, add:

```markdown
## [0.1.33] - 2026-09-26

### Added

- Internal groundwork for native LiveKit video calls: `Event`/recurring
  series now carry a provider-agnostic meeting reference behind a new
  `MeetingProvider` interface. No visible behavior changes — the existing
  "Online session" link field works exactly as before.
```

(Use today's actual date if this task runs on a different day.)

- [ ] **Step 3: Check translations**

This plan adds no new user-visible `tr()` strings (Q3: no UI changes). Confirm this holds before committing:

```bash
cmake --build build-release --target update_translations
/usr/bin/git status translation/
```

Expected: `git status` shows no changes under `translation/` (or, if the scan finds something unexpected, translate any `type="unfinished"` entries in `translation/app_ru.ts` and `translation/app_en.ts` before committing, per `AGENTS.md`).

- [ ] **Step 4: Run the full test suite one more time**

Run: `cmake -S . -B build -DPCM_BUILD_TESTS=ON && cmake --build build --parallel && ctest --test-dir build --output-on-failure`
Expected: all tests pass, including every test added in Tasks 1–5.

- [ ] **Step 5: Commit**

```bash
/usr/bin/git add CMakeLists.txt src/app/application.cpp CHANGELOG.md
/usr/bin/git commit -m "chore: bump version to 0.1.33 for meeting provider domain layer"
```
