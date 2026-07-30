# Appointment Confirmation Tracking Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a psychologist mark a session confirmed/unconfirmed independently of `event_stat_id`, see it on the Timeline card, filter the day view to unconfirmed sessions, and copy a "please confirm" message using a configurable template.

**Architecture:** A new nullable `confirmed_at` column on `Event` (additive migration, no schema_version bump), threaded through the existing `DuckEvent`/`add_event`/`update_event` plumbing with no new `Database` method. `QTimelineModel::toggleEventConfirmed(id)` flips it, materializing virtual recurring occurrences via the same path single-occurrence edits already use. The UI path (context-menu action → signal → `QEventView` → `QTimelineWidget` → `event_info.cpp`) exactly mirrors the existing delete-event wiring. A new `pcm::confirmation` module (`src/widgets/confirmation_utils.h/.cpp`) mirrors the existing `pcm::meeting` module for building/copying the confirmation-request text from a settings-configurable template.

**Tech Stack:** C++20, Qt6 (Core, Widgets), DuckDB, GoogleTest.

## Global Constraints

- Version bump required in every MR: `CMakeLists.txt` and `src/app/application.cpp` — bump 0.1.16 → 0.1.17.
- CHANGELOG.md entry required every MR (`AGENTS.md`).
- Database schema changes require a migration and a restore/round-trip test (`AGENTS.md`) — satisfied by Task 1's DB round-trip test and Task 2's backup/restore round-trip test.
- Two-space indent, PascalCase classes, camelCase Qt methods, snake_case DB APIs (`AGENTS.md`).
- Do not develop directly on `main` — branch `feat/21-appointment-confirmation` (already created off `main`).
- Out of scope (per issue #21 and the approved design): sending the confirmation request (SMS/Telegram/email); removing/changing the existing `event_stat_id=4` "confirmed" status; a cross-day unconfirmed-sessions overview (belongs to separate issue #27).
- No dedicated unit test for `confirmation_utils.cpp`, mirroring `meeting_utils.cpp`'s existing precedent of build-only verification (no test target links `PsyClientManager_widgets`).

---

### Task 1: `confirmed_at` column, `DuckEvent` field, and DB round-trip test

**Files:**
- Modify: `src/database/constants.hpp`
- Modify: `src/database/schema.hpp`
- Modify: `src/database/database.cpp`
- Test: `test/database_tests.cpp`

**Interfaces:**
- Produces: `DuckEvent::confirmed_at` (`std::optional<std::int64_t>`), persisted transparently by the existing `Database::add_event`/`Database::update_event`. Used by Task 3 (`QTimelineModel`) and Task 2 (restore round-trip test).

- [ ] **Step 1: Write the failing test**

Append to `test/database_tests.cpp`, after the `AddClientAndEvent` test (after line 141):

```cpp
TEST(DatabaseTest, PersistsConfirmedAtOnEvent) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_confirmed_at")}};

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

  const auto unconfirmed = db.get_event(eventId);
  ASSERT_NE(unconfirmed, nullptr);
  EXPECT_FALSE(unconfirmed->confirmed_at.has_value());

  auto toConfirm = *unconfirmed;
  toConfirm.confirmed_at = 1730000500000;
  ASSERT_TRUE(db.update_event(toConfirm));

  const auto confirmed = db.get_event(eventId);
  ASSERT_NE(confirmed, nullptr);
  ASSERT_TRUE(confirmed->confirmed_at.has_value());
  EXPECT_EQ(*confirmed->confirmed_at, 1730000500000);

  auto toUnconfirm = *confirmed;
  toUnconfirm.confirmed_at = std::nullopt;
  ASSERT_TRUE(db.update_event(toUnconfirm));

  const auto unconfirmedAgain = db.get_event(eventId);
  ASSERT_NE(unconfirmedAgain, nullptr);
  EXPECT_FALSE(unconfirmedAgain->confirmed_at.has_value());

  db_dir.remove(true);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-release --target PsyClientManager_database_tests --parallel`
Expected: FAIL to compile — `DuckEvent` has no member `confirmed_at`.

- [ ] **Step 3: Add the migration**

In `src/database/constants.hpp`, inside `kCreateTables`, right after the two `UPDATE Event SET buffer_...` lines:

```cpp
UPDATE Event SET buffer_before_minutes = 0 WHERE buffer_before_minutes IS NULL;
UPDATE Event SET buffer_after_minutes = 0 WHERE buffer_after_minutes IS NULL;
ALTER TABLE Event ADD COLUMN IF NOT EXISTS confirmed_at TIMESTAMP;
```

- [ ] **Step 4: Extend `kInsertEventQuery` and `kUpdateEventQuery`**

In `src/database/constants.hpp`, replace `kInsertEventQuery`:

```cpp
constexpr auto kInsertEventQuery = R"duckdb(
INSERT INTO Event (
    id,
    name, description, is_work_event,
    event_stat_id, payment_stat_id,
    start_date, end_date, duration, cost,
    is_online, meeting_url, series_id, original_occurrence_start,
    cancellation_reason, canceled_by, buffer_before_minutes, buffer_after_minutes,
    confirmed_at
)
SELECT
    COALESCE(MAX(id), 0) + 1,
    $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13, $14, $15, $16, $17, $18
FROM Event
RETURNING id
)duckdb";
```

Replace `kUpdateEventQuery`:

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
    confirmed_at = $18,
    reminder_notified_at = CASE
        WHEN start_date IS DISTINCT FROM $6 OR end_date IS DISTINCT FROM $7 THEN NULL
        ELSE reminder_notified_at
    END
WHERE id = $19
)duckdb";
```

- [ ] **Step 5: Add the `DuckEvent` field**

In `src/database/schema.hpp`, in `struct DuckEvent`, after `bool is_virtual_occurrence = false;`:

```cpp
  bool is_virtual_occurrence = false;
  std::optional<std::int64_t> confirmed_at = std::nullopt;
```

In the same struct's chunk-based constructor, after the `readBufferMinutes(...)` call:

```cpp
    readBufferMinutes(chunk, index, 17, 18, buffer_before_minutes,
                      buffer_after_minutes);
    if (chunk.ColumnCount() > 19) {
      confirmed_at = db_utils::toOptionalTimestampMs(chunk.GetValue(19, index));
    }
  }
};
```

- [ ] **Step 6: Bind the new parameter in `Database::add_event`/`update_event`**

In `src/database/database.cpp`, in `Database::add_event`, append to the bind list:

```cpp
       duckdb::Value::INTEGER(static_cast<int32_t>(event.buffer_before_minutes)),
       duckdb::Value::INTEGER(static_cast<int32_t>(event.buffer_after_minutes)),
       timestampMsOrNull(event.confirmed_at)});
```

In `Database::update_event`, insert before the trailing `duckdb::Value::BIGINT(event.id)`:

```cpp
       duckdb::Value::INTEGER(static_cast<int32_t>(event.buffer_before_minutes)),
       duckdb::Value::INTEGER(static_cast<int32_t>(event.buffer_after_minutes)),
       timestampMsOrNull(event.confirmed_at),
       duckdb::Value::BIGINT(event.id)});
```

- [ ] **Step 7: Run test to verify it passes**

Run: `cmake --build build-release --target PsyClientManager_database_tests --parallel && ./build-release/test/PsyClientManager_database_tests --gtest_filter=DatabaseTest.PersistsConfirmedAtOnEvent`
Expected: PASS

- [ ] **Step 8: Run the full database test suite as a regression check**

Run: `./build-release/test/PsyClientManager_database_tests`
Expected: all tests pass.

- [ ] **Step 9: Commit**

```bash
git add src/database/constants.hpp src/database/schema.hpp src/database/database.cpp test/database_tests.cpp
git commit -m "feat(database): add confirmed_at tracking to Event"
```

---

### Task 2: Restore round-trip test for `confirmed_at`

**Files:**
- Test: `test/backup_tests.cpp`

**Interfaces:**
- Consumes: `DuckEvent::confirmed_at` (Task 1), existing `BackupService`/`RestoreService`, existing `makeTestDatabase` test helper.

- [ ] **Step 1: Write the test**

Append to `test/backup_tests.cpp`, after `RestoresSeriesOccurrenceReminderState` (after its closing brace, following line ~756):

```cpp
TEST(RestoreServiceTest, RestoresEventConfirmedAt) {
  auto sourceDb = makeTestDatabase("tmp_restore_confirmed_source");
  DuckEvent event;
  event.name = std::string{"Session"};
  event.start_date = 1730000000000;
  event.end_date = 1730003600000;
  const auto eventId = sourceDb.add_event(event);
  ASSERT_GT(eventId, 0);

  auto toConfirm = *sourceDb.get_event(eventId);
  toConfirm.confirmed_at = 1730000500000;
  ASSERT_TRUE(sourceDb.update_event(toConfirm));

  const auto backupPath = Poco::Path(Poco::Path::current())
                              .append("tmp_restore_confirmed.psybackup")
                              .toString();
  if (Poco::File(backupPath).exists()) {
    Poco::File(backupPath).remove();
  }
  ASSERT_TRUE(pcm::backup::BackupService{}.create_backup(sourceDb, backupPath).ok);

  const auto targetPath = Poco::Path(Poco::Path::current())
                              .append("tmp_restore_confirmed_target")
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
  const auto restoredEvent = restoredDb.get_event(eventId);
  ASSERT_NE(restoredEvent, nullptr);
  ASSERT_TRUE(restoredEvent->confirmed_at.has_value());
  EXPECT_EQ(*restoredEvent->confirmed_at, 1730000500000);

  Poco::File(backupPath).remove();
  Poco::File(targetPath).remove(true);
  Poco::File(Poco::Path(Poco::Path::current()).append("tmp_restore_confirmed_source"))
      .remove(true);
}
```

- [ ] **Step 2: Build and run**

Run: `cmake --build build-release --target PsyClientManager_backup_tests --parallel && ./build-release/test/PsyClientManager_backup_tests --gtest_filter=RestoreServiceTest.RestoresEventConfirmedAt`
Expected: PASS

- [ ] **Step 3: Run the full backup test suite as a regression check**

Run: `./build-release/test/PsyClientManager_backup_tests`
Expected: all tests pass.

- [ ] **Step 4: Commit**

```bash
git add test/backup_tests.cpp
git commit -m "test(backup): verify confirmed_at survives backup/restore"
```

---

### Task 3: `QTimelineModel::toggleEventConfirmed` and `setUnconfirmedOnlyFilter`

**Files:**
- Modify: `src/event_view/qtimeline_model.h`
- Modify: `src/event_view/qtimeline_model.cpp`

**Interfaces:**
- Produces: `void QTimelineModel::toggleEventConfirmed(int64_t id)`, `void QTimelineModel::setUnconfirmedOnlyFilter(bool enabled)`. Used by Task 6 (wiring) and Task 7 (Timeline checkbox).

- [ ] **Step 1: Declare the new methods**

In `src/event_view/qtimeline_model.h`, after `void removeEvent(int64_t id);`:

```cpp
  void removeEvent(int64_t id);
  void toggleEventConfirmed(int64_t id);
  void setUnconfirmedOnlyFilter(bool enabled);
```

Add the new member after `mCurrentDate`:

```cpp
  QVector<DuckEvent> mEvents;
  QDate mCurrentDate;
  bool mUnconfirmedOnlyFilter = false;
```

- [ ] **Step 2: Implement `toggleEventConfirmed`**

In `src/event_view/qtimeline_model.cpp`, after `QTimelineModel::removeEvent` (after its closing brace, following line 300):

```cpp
void QTimelineModel::toggleEventConfirmed(const int64_t id) {
  for (int i = 0; i < mEvents.size(); ++i) {
    if (mEvents[i].id != id) {
      continue;
    }

    DuckEvent updated = mEvents[i];
    updated.confirmed_at = updated.confirmed_at.has_value()
                               ? std::nullopt
                               : std::make_optional(
                                     QDateTime::currentMSecsSinceEpoch());

    if (updated.is_virtual_occurrence) {
      updated.id = -1;
      const auto newId = mDb->add_event(updated, true);
      if (newId <= 0) {
        qWarning() << "QTimelineModel::toggleEventConfirmed failed to "
                      "materialize occurrence for id="
                   << id;
        return;
      }
      updated.id = newId;
      updated.is_virtual_occurrence = false;
    } else if (!mDb->update_event(updated, true)) {
      qWarning() << "QTimelineModel::toggleEventConfirmed failed for id=" << id;
      return;
    }

    mEvents[i] = updated;
    emit dataChanged(index(i, 0, QModelIndex()), index(i, 0, QModelIndex()),
                     {EventDataRole});
    return;
  }
}

void QTimelineModel::setUnconfirmedOnlyFilter(const bool enabled) {
  if (mUnconfirmedOnlyFilter == enabled) {
    return;
  }
  mUnconfirmedOnlyFilter = enabled;
  loadEventsForDay(mCurrentDate);
}
```

- [ ] **Step 3: Apply the filter in `loadEventsForDay`**

In `src/event_view/qtimeline_model.cpp`, in `loadEventsForDay`, insert right before the final `std::sort(...)` call:

```cpp
  if (mUnconfirmedOnlyFilter) {
    mEvents.erase(std::remove_if(mEvents.begin(), mEvents.end(),
                                 [](const DuckEvent &event) {
                                   return event.confirmed_at.has_value();
                                 }),
                 mEvents.end());
  }

  std::sort(mEvents.begin(), mEvents.end(), [](const DuckEvent &left, const DuckEvent &right) {
    return left.start_date.value_or(0) < right.start_date.value_or(0);
  });
```

- [ ] **Step 4: Build**

Run: `cmake --build build-release --target PsyClientManager --parallel`
Expected: builds cleanly.

- [ ] **Step 5: Run the full test suite as a regression check**

Run: `ctest --test-dir build-release --output-on-failure`
Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/event_view/qtimeline_model.h src/event_view/qtimeline_model.cpp
git commit -m "feat(timeline): add confirm-toggle and unconfirmed-only filter to QTimelineModel"
```

---

### Task 4: `confirmation_utils` module and `app_settings` template

**Files:**
- Create: `src/widgets/confirmation_utils.h`
- Create: `src/widgets/confirmation_utils.cpp`
- Modify: `src/widgets/CMakeLists.txt`
- Modify: `src/widgets/app_settings.h`
- Modify: `src/widgets/app_settings.cpp`

**Interfaces:**
- Produces: `QString pcm::confirmation::buildConfirmationRequestText(const QString &clientName, qint64 startDateTimeUtcMs)`, `void pcm::confirmation::copyConfirmationRequest(const QString &clientName, qint64 startDateTimeUtcMs)`; `QString pcm::app_settings::confirmationRequestTemplate()` / `void setConfirmationRequestTemplate(const QString &)`. Used by Task 5 (`QEventItem` context menu) and Task 7 (Settings UI).

- [ ] **Step 1: Add the settings key and default**

In `src/widgets/app_settings.cpp`, after `kMeetingInviteTemplateKey`:

```cpp
constexpr auto kMeetingInviteTemplateKey = "online/meetingInviteTemplate";
constexpr auto kConfirmationRequestTemplateKey = "event/confirmationRequestTemplate";
```

After `defaultMeetingInviteTemplateValue()`'s closing brace, still in the anonymous namespace:

```cpp
QString defaultConfirmationRequestTemplateValue() {
  return QObject::tr("Hello, {client_name}!\n\n"
                     "Please confirm your session on {date} at {time}.\n\n"
                     "Thank you!");
}
```

- [ ] **Step 2: Declare and implement the getter/setter**

In `src/widgets/app_settings.h`, after `void setMeetingInviteTemplate(const QString &templateText);`:

```cpp
QString meetingInviteTemplate();
void setMeetingInviteTemplate(const QString &templateText);
QString confirmationRequestTemplate();
void setConfirmationRequestTemplate(const QString &templateText);
```

In `src/widgets/app_settings.cpp`, after `setMeetingInviteTemplate`'s closing brace:

```cpp
void setMeetingInviteTemplate(const QString &templateText) {
  QSettings settings;
  settings.setValue(kMeetingInviteTemplateKey, templateText);
}

QString confirmationRequestTemplate() {
  QSettings settings;
  return settings
      .value(kConfirmationRequestTemplateKey, defaultConfirmationRequestTemplateValue())
      .toString();
}

void setConfirmationRequestTemplate(const QString &templateText) {
  QSettings settings;
  settings.setValue(kConfirmationRequestTemplateKey, templateText);
}
```

- [ ] **Step 3: Create `confirmation_utils.h`**

`src/widgets/confirmation_utils.h`:

```cpp
#pragma once

#include <QString>

namespace pcm::confirmation {

QString buildConfirmationRequestText(const QString &clientName,
                                     qint64 startDateTimeUtcMs);
void copyConfirmationRequest(const QString &clientName,
                             qint64 startDateTimeUtcMs);

} // namespace pcm::confirmation
```

- [ ] **Step 4: Implement `confirmation_utils.cpp`**

`src/widgets/confirmation_utils.cpp`:

```cpp
#include "confirmation_utils.h"

#include "app_settings.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QLocale>
#include <QTimeZone>

namespace pcm::confirmation {

QString buildConfirmationRequestText(const QString &clientName,
                                     const qint64 startDateTimeUtcMs) {
  const auto startDateTime =
      QDateTime::fromMSecsSinceEpoch(startDateTimeUtcMs, QTimeZone::UTC).toLocalTime();
  const auto date = QLocale().toString(startDateTime.date(), QLocale::ShortFormat);
  const auto time = QLocale().toString(startDateTime.time(), QLocale::ShortFormat);

  auto text = pcm::app_settings::confirmationRequestTemplate();
  text.replace(QStringLiteral("{client_name}"), clientName.trimmed());
  text.replace(QStringLiteral("{date}"), date);
  text.replace(QStringLiteral("{time}"), time);
  return text;
}

void copyConfirmationRequest(const QString &clientName,
                             const qint64 startDateTimeUtcMs) {
  if (auto *clipboard = QApplication::clipboard()) {
    clipboard->setText(buildConfirmationRequestText(clientName, startDateTimeUtcMs));
  }
}

} // namespace pcm::confirmation
```

- [ ] **Step 5: Register the new files in CMake**

In `src/widgets/CMakeLists.txt`, add the two new files to the `qt_add_library` sources list:

```cmake
qt_add_library(${TARGET_NAME} STATIC
        app_settings.cpp
        meeting_utils.cpp
        meeting_utils.h
        confirmation_utils.cpp
        confirmation_utils.h
        tab_button.cpp
        rounded_calendar_widget.cpp
        quick_slots_widget.cpp
)
```

- [ ] **Step 6: Build**

Run: `cmake --build build-release --target PsyClientManager_widgets --parallel`
Expected: builds cleanly.

- [ ] **Step 7: Commit**

```bash
git add src/widgets/confirmation_utils.h src/widgets/confirmation_utils.cpp src/widgets/CMakeLists.txt src/widgets/app_settings.h src/widgets/app_settings.cpp
git commit -m "feat(widgets): add confirmation-request template and clipboard copy"
```

---

### Task 5: `QEventItem` — confirmed indicator and context menu actions

**Files:**
- Modify: `src/event_view/event_item.h`
- Modify: `src/event_view/event_item.cpp`

**Interfaces:**
- Consumes: `pcm::confirmation::copyConfirmationRequest` (Task 4).
- Produces: `QEventItem::confirmToggleRequested()` signal, `QEventItem::isConfirmed() const`. Used by Task 6.

- [ ] **Step 1: Add the member, signal, and accessor**

In `src/event_view/event_item.h`, add the signal after `deleteRequested();`:

```cpp
  void itemSelected();
  void editRequested();
  void deleteRequested();
  void confirmToggleRequested();
```

Add the accessor after `bufferAfterMinutes()`:

```cpp
  [[nodiscard]] int64_t bufferAfterMinutes() const;
  [[nodiscard]] bool isConfirmed() const;
```

Add the member after `mIsVirtualOccurrence`:

```cpp
  bool mIsVirtualOccurrence = false;
  std::optional<int64_t> mConfirmedAt;
```

- [ ] **Step 2: Thread `confirmed_at` through the constructors, `updateFromEvent`, and `toEvent`**

In `src/event_view/event_item.cpp`, in `updateFromEvent`, `QEventItem(const DuckEvent &event)`, and `toEvent()`, add the `confirmed_at` line right next to the existing `is_virtual_occurrence` lines (three call sites total — two reads, one write):

In `updateFromEvent` and the `QEventItem(const DuckEvent &event)` constructor (both currently read):
```cpp
  mIsVirtualOccurrence = event.is_virtual_occurrence;
  mConfirmedAt = event.confirmed_at;
```

In `toEvent()` (write):
```cpp
  event.is_virtual_occurrence = mIsVirtualOccurrence;
  event.confirmed_at = mConfirmedAt;
```

Add the accessor implementation after `bufferAfterMinutes()`'s definition (near the other simple getters, e.g. after `bool QEventItem::isWorkItem() const { return mIsWorkItem; };`):

```cpp
bool QEventItem::isConfirmed() const { return mConfirmedAt.has_value(); }
```

- [ ] **Step 3: Add the context menu actions**

In `src/event_view/event_item.cpp`, in `contextMenuEvent`, insert a new block right after the closing `menu.addSeparator();` of the `if (mIsOnline)` block and before `auto *editAction = ...`:

```cpp
  if (mIsWorkItem) {
    auto *toggleConfirmAction = menu.addAction(
        mConfirmedAt.has_value() ? tr("Mark unconfirmed") : tr("Mark confirmed"));
    connect(toggleConfirmAction, &QAction::triggered, this,
            [this]() { emit confirmToggleRequested(); });
    auto *copyConfirmationRequestAction = menu.addAction(tr("Copy confirmation request"));
    connect(copyConfirmationRequestAction, &QAction::triggered, this, [this]() {
      pcm::confirmation::copyConfirmationRequest(
          mClientName, mStartTime.toUTC().toMSecsSinceEpoch());
    });
    menu.addSeparator();
  }
```

In `src/event_view/event_item.cpp`, add the include next to the existing `meeting_utils.h` include at the top of the file:

```cpp
#include "event_item.h"
#include "../widgets/app_settings.h"
#include "../widgets/confirmation_utils.h"
#include "../widgets/meeting_utils.h"
```

- [ ] **Step 4: Draw the confirmed badge**

In `src/event_view/event_item.cpp`, in `paint()`, right after `painter->drawRoundedRect(x, 0, mSize.width(), mSize.height(), 5, 5);` and before `painter->setPen(Qt::white);`:

```cpp
  painter->drawRoundedRect(x, 0, mSize.width(), mSize.height(), 5, 5);

  if (mConfirmedAt.has_value()) {
    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(46, 204, 113));
    painter->drawEllipse(QPointF(x + mSize.width() - 8.0, 8.0), 4.0, 4.0);
    painter->restore();
  }

  painter->setPen(Qt::white);
```

- [ ] **Step 5: Build**

Run: `cmake --build build-release --target PsyClientManager --parallel`
Expected: builds cleanly.

- [ ] **Step 6: Commit**

```bash
git add src/event_view/event_item.h src/event_view/event_item.cpp
git commit -m "feat(timeline): add confirmed indicator and quick actions to event cards"
```

---

### Task 6: Wire the confirm-toggle signal chain

**Files:**
- Modify: `src/event_view/event_view.h`
- Modify: `src/event_view/event_view.cpp`
- Modify: `src/timeline_widget/timeline_widget.h`
- Modify: `src/timeline_widget/timeline_widget.cpp`
- Modify: `src/pages/event_info_page/event_info.h`
- Modify: `src/pages/event_info_page/event_info.cpp`

**Interfaces:**
- Consumes: `QEventItem::confirmToggleRequested()` (Task 5), `QTimelineModel::toggleEventConfirmed(int64_t)` (Task 3).
- Produces: `QEventView::eventConfirmToggleRequested(int64_t)`, `QTimelineWidget::eventConfirmToggleRequested(int64_t)` + `QTimelineWidget::toggleEventConfirmed(int64_t) const`.

- [ ] **Step 1: `QEventView`**

In `src/event_view/event_view.h`, add the signal after `eventDeleteRequested`:

```cpp
  void eventDeleteRequested(int64_t eventId);
  void eventConfirmToggleRequested(int64_t eventId);
```

Add the private slot after `onEventDeleteRequested`:

```cpp
  void onEventDeleteRequested();
  void onEventConfirmToggleRequested();
```

In `src/event_view/event_view.cpp`, add the connect call at **both** places `QEventItem::deleteRequested` is connected (in `onRowsInserted`, ~line 84, and in `onModelReset`, ~line 173):

```cpp
    connect(item, &QEventItem::deleteRequested, this,
            &QEventView::onEventDeleteRequested);
    connect(item, &QEventItem::confirmToggleRequested, this,
            &QEventView::onEventConfirmToggleRequested);
```

Add the slot implementation after `onEventDeleteRequested()`:

```cpp
void QEventView::onEventConfirmToggleRequested() {
  auto *item = qobject_cast<QEventItem *>(sender());
  if (item != nullptr) {
    emit eventConfirmToggleRequested(item->getId());
  }
}
```

- [ ] **Step 2: `QTimelineWidget`**

In `src/timeline_widget/timeline_widget.h`, add the signal after `eventDeleteRequested`:

```cpp
    void eventDeleteRequested(int64_t eventId);
    void eventConfirmToggleRequested(int64_t eventId);
```

Add the method after `removeEvent`:

```cpp
    void removeEvent(int64_t id) const;
    void toggleEventConfirmed(int64_t id) const;
```

In `src/timeline_widget/timeline_widget.cpp`, add the connect call after the existing `eventDeleteRequested` connect in the constructor:

```cpp
  connect(mEventView, &QEventView::eventDeleteRequested, this,
          &QTimelineWidget::eventDeleteRequested);
  connect(mEventView, &QEventView::eventConfirmToggleRequested, this,
          &QTimelineWidget::eventConfirmToggleRequested);
```

Add the method implementation after `removeEvent`:

```cpp
void QTimelineWidget::removeEvent(const int64_t id) const {
  if (!mModel)
    return;
  mModel->removeEvent(id);
}

void QTimelineWidget::toggleEventConfirmed(const int64_t id) const {
  if (!mModel)
    return;
  mModel->toggleEventConfirmed(id);
}
```

- [ ] **Step 3: `event_info.cpp`**

In `src/pages/event_info_page/event_info.h`, add the slot after `onTimelineEventDeleteRequested`:

```cpp
  void onTimelineEventDeleteRequested(int64_t eventId);
  void onTimelineEventConfirmToggleRequested(int64_t eventId);
```

In `src/pages/event_info_page/event_info.cpp`, in `connectSignals()`, add after the `eventDeleteRequested` connect:

```cpp
  connect(mTimelineWidget, &QTimelineWidget::eventDeleteRequested, this,
          &QEventInfoPage::onTimelineEventDeleteRequested);
  connect(mTimelineWidget, &QTimelineWidget::eventConfirmToggleRequested, this,
          &QEventInfoPage::onTimelineEventConfirmToggleRequested);
```

Add the slot implementation after `onTimelineEventEditRequested` (before `onTimelineEventDeleteRequested`, or anywhere among the sibling handlers):

```cpp
void QEventInfoPage::onTimelineEventConfirmToggleRequested(const int64_t eventId) {
  if (mTimelineWidget) {
    mTimelineWidget->toggleEventConfirmed(eventId);
  }
}
```

- [ ] **Step 4: Build**

Run: `cmake --build build-release --target PsyClientManager --parallel`
Expected: builds cleanly.

- [ ] **Step 5: Run the full test suite as a regression check**

Run: `ctest --test-dir build-release --output-on-failure`
Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
git add src/event_view/event_view.h src/event_view/event_view.cpp src/timeline_widget/timeline_widget.h src/timeline_widget/timeline_widget.cpp src/pages/event_info_page/event_info.h src/pages/event_info_page/event_info.cpp
git commit -m "feat(timeline): wire confirm-toggle action from event card to the model"
```

---

### Task 7: Settings UI and Timeline filter checkbox

**Files:**
- Modify: `src/app/settings_dialog.h`
- Modify: `src/app/settings_dialog.cpp`
- Modify: `src/timeline_widget/timeline_widget.h`
- Modify: `src/timeline_widget/timeline_widget.cpp`

**Interfaces:**
- Consumes: `pcm::app_settings::confirmationRequestTemplate`/`setConfirmationRequestTemplate` (Task 4), `QTimelineModel::setUnconfirmedOnlyFilter` (Task 3).

- [ ] **Step 1: Add the template editor member and include**

In `src/app/settings_dialog.h`, add the member after `mMeetingInviteTemplateEdit`:

```cpp
  QTextEdit *mMeetingInviteTemplateEdit{nullptr};
  QTextEdit *mConfirmationRequestTemplateEdit{nullptr};
```

- [ ] **Step 2: Build the "Confirmation request template" group box**

In `src/app/settings_dialog.cpp`, insert right before `eventSettingsLayout->addStretch();` (after the existing `eventsBox` is added to `eventSettingsLayout`, following line 375):

```cpp
  eventSettingsLayout->addWidget(eventsBox);

  auto *confirmationBox = new QGroupBox(tr("Confirmation request template"), eventsPage);
  auto *confirmationLayout = new QVBoxLayout(confirmationBox);
  confirmationLayout->setContentsMargins(16, 16, 16, 16);
  confirmationLayout->setSpacing(10);
  auto *confirmationTemplateDescription = new QLabel(
      tr("Available variables: {client_name}, {date}, {time}"), confirmationBox);
  confirmationTemplateDescription->setWordWrap(true);
  confirmationTemplateDescription->setStyleSheet("color: rgba(255, 255, 255, 0.68);");
  mConfirmationRequestTemplateEdit = new QTextEdit(confirmationBox);
  mConfirmationRequestTemplateEdit->setAcceptRichText(false);
  mConfirmationRequestTemplateEdit->setMinimumHeight(100);
  confirmationLayout->addWidget(confirmationTemplateDescription);
  confirmationLayout->addWidget(mConfirmationRequestTemplateEdit);
  eventSettingsLayout->addWidget(confirmationBox);
  eventSettingsLayout->addStretch();
```

(This replaces the existing `eventSettingsLayout->addWidget(eventsBox);` / `eventSettingsLayout->addStretch();` pair — keep the first line as-is, insert the new block between it and the `addStretch()` call.)

- [ ] **Step 3: Load and connect the new editor**

In `src/app/settings_dialog.cpp`, in `loadSettings()`, after `mMeetingInviteTemplateEdit->setPlainText(...)`:

```cpp
  mMeetingInviteTemplateEdit->setPlainText(
      pcm::app_settings::meetingInviteTemplate());
  mConfirmationRequestTemplateEdit->setPlainText(
      pcm::app_settings::confirmationRequestTemplate());
```

In `connectSignals()`, after the existing `mMeetingInviteTemplateEdit` connect:

```cpp
  connect(mMeetingInviteTemplateEdit, &QTextEdit::textChanged, this, [this]() {
    pcm::app_settings::setMeetingInviteTemplate(
        mMeetingInviteTemplateEdit->toPlainText());
  });
  connect(mConfirmationRequestTemplateEdit, &QTextEdit::textChanged, this, [this]() {
    pcm::app_settings::setConfirmationRequestTemplate(
        mConfirmationRequestTemplateEdit->toPlainText());
  });
```

- [ ] **Step 4: Add the "Show only unconfirmed" checkbox to the Timeline**

In `src/timeline_widget/timeline_widget.h`, add the include and member:

```cpp
#include <QCheckBox>
```

```cpp
private:
    QVBoxLayout *mLayout = nullptr;
    QCheckBox *mUnconfirmedOnlyCheckBox = nullptr;
    QEventView *mEventView = nullptr;
    QTimelineModel *mModel = nullptr;
```

In `src/timeline_widget/timeline_widget.cpp`, in the constructor, before `mEventView = new QEventView(this);`:

```cpp
  mUnconfirmedOnlyCheckBox = new QCheckBox(tr("Show only unconfirmed"), this);
  mLayout->addWidget(mUnconfirmedOnlyCheckBox);

  mEventView = new QEventView(this);
  mEventView->setModel(mModel);

  mLayout->addWidget(mEventView);

  connect(mUnconfirmedOnlyCheckBox, &QCheckBox::toggled, mModel,
          &QTimelineModel::setUnconfirmedOnlyFilter);
```

- [ ] **Step 5: Build**

Run: `cmake --build build-release --target PsyClientManager --parallel`
Expected: builds cleanly.

- [ ] **Step 6: Run the full test suite as a regression check**

Run: `ctest --test-dir build-release --output-on-failure`
Expected: all tests pass.

- [ ] **Step 7: Commit**

```bash
git add src/app/settings_dialog.h src/app/settings_dialog.cpp src/timeline_widget/timeline_widget.h src/timeline_widget/timeline_widget.cpp
git commit -m "feat(settings): add confirmation template editor and unconfirmed-only filter"
```

---

### Task 8: Version bump and CHANGELOG

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/app/application.cpp`
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Bump the version**

In `CMakeLists.txt`:

```cmake
project(PsyClientManager VERSION 0.1.17 LANGUAGES CXX)
```

In `src/app/application.cpp`:

```cpp
  app.setApplicationVersion("0.1.17");
```

- [ ] **Step 2: Add the CHANGELOG entry**

In `CHANGELOG.md`, add above the `[0.1.16]` entry:

```markdown
## [0.1.17] - 2026-07-30

### Added

- Appointment confirmation tracking: mark a session confirmed/unconfirmed
  independently of its status, see it on the Timeline card, filter the day
  view to unconfirmed sessions, and copy a "please confirm" message using a
  configurable template.
```

- [ ] **Step 3: Build and run full test suite**

Run: `cmake --build build-release --parallel && ctest --test-dir build-release --output-on-failure`
Expected: builds cleanly, all tests pass.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt src/app/application.cpp CHANGELOG.md
git commit -m "chore: bump version to 0.1.17"
```
