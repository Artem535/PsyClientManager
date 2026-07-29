# Automatic Backups with Retention Policy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add scheduled automatic backups (interval-based, plus a shutdown safety-net check) with count-based retention, on top of the existing manual `.psybackup` flow.

**Architecture:** A pure-C++ `BackupRotationService` (prunes old auto-backup files by count) and a pure `isAutoBackupDue()` free function both live in the Qt-free `src/backup/` library, matching `BackupService`/`RestoreService`'s existing style and staying unit-testable via the existing `backup_tests` executable. A new `AutoBackupScheduler` QObject — which needs `QTimer`/`QThread` and therefore Qt — lives in `src/app/` (which already depends on Qt6) instead of `src/backup/` (which currently has zero Qt dependency; adding one there would mean retrofitting the whole static library to build with AUTOMOC). `AutoBackupScheduler` is owned by `Application`, orchestrates a background-thread backup via the same `QThread`-worker shape `SettingsDialog::createBackup()` already uses, then calls `BackupRotationService::prune`.

**Tech Stack:** C++20, Qt6 (Core, Widgets), Poco (File/Path/DirectoryIterator), GoogleTest.

## Global Constraints

- Version bump required in every MR: `CMakeLists.txt` (`project(... VERSION ...)`) and `src/app/application.cpp` (`app.setApplicationVersion(...)`) — bump 0.1.15 → 0.1.16.
- CHANGELOG.md entry required every MR (`AGENTS.md`).
- Two-space indent, PascalCase classes, camelCase Qt methods, snake_case DB/service APIs (`AGENTS.md`).
- Do not develop directly on `main` — branch `feat/25-automatic-backups` off `main` before starting.
- Out of scope (per issue #25 and the approved design): cloud backup destinations, encrypted backups, pre-migration automatic backups.
- Automatic backups must never block the UI thread (acceptance criterion) — all actual backup I/O runs on a background `QThread`, same pattern as the existing manual "Create backup..." button.
- A failed automatic backup must not crash the app or corrupt existing backups (acceptance criterion) — failures are logged only, never surfaced as a blocking dialog, and never touch `autoBackupLastRunAtMs` (so the next check retries) or existing backup files (the underlying `BackupService::create_backup` already writes via temp-file-then-atomic-rename).

---

### Task 1: `BackupRotationService`

**Files:**
- Create: `src/backup/backup_rotation_service.h`
- Create: `src/backup/backup_rotation_service.cpp`
- Modify: `src/backup/CMakeLists.txt`
- Test: `test/backup_tests.cpp`

**Interfaces:**
- Produces: `pcm::backup::RotationResult` (`bool ok`, `std::string error`, `int removed_count`) and `pcm::backup::BackupRotationService::prune(const std::string &directory, const std::string &filename_prefix, int keep_count) -> RotationResult`. Used by Task 4.

- [ ] **Step 1: Write the failing test**

Append to `test/backup_tests.cpp` (after the last test, following line 754; the file has no `int main` — it links `GTest::gtest_main`):

```cpp
TEST(BackupRotationServiceTest, KeepsNewestAndRemovesOlderMatchingFiles) {
  const auto dir =
      Poco::Path(Poco::Path::current()).append("tmp_rotation_dir").toString();
  if (Poco::File(dir).exists()) {
    Poco::File(dir).remove(true);
  }
  Poco::File(dir).createDirectories();

  const std::vector<std::string> autoNames = {
      "PsyClientManager-auto-20260101-000000.psybackup",
      "PsyClientManager-auto-20260102-000000.psybackup",
      "PsyClientManager-auto-20260103-000000.psybackup",
      "PsyClientManager-auto-20260104-000000.psybackup",
      "PsyClientManager-auto-20260105-000000.psybackup",
  };
  for (const auto &name : autoNames) {
    std::ofstream(Poco::Path(dir).append(name).toString()) << "x";
  }
  // A manual backup in the same directory must never be touched by rotation.
  const auto manualName =
      Poco::Path(dir).append("PsyClientManager-20260106-000000.psybackup").toString();
  std::ofstream(manualName) << "x";

  pcm::backup::BackupRotationService service;
  const auto result = service.prune(dir, "PsyClientManager-auto-", 3);
  ASSERT_TRUE(result.ok) << result.error;
  EXPECT_EQ(result.removed_count, 2);

  EXPECT_FALSE(Poco::File(Poco::Path(dir).append(autoNames[0]).toString()).exists());
  EXPECT_FALSE(Poco::File(Poco::Path(dir).append(autoNames[1]).toString()).exists());
  EXPECT_TRUE(Poco::File(Poco::Path(dir).append(autoNames[2]).toString()).exists());
  EXPECT_TRUE(Poco::File(Poco::Path(dir).append(autoNames[3]).toString()).exists());
  EXPECT_TRUE(Poco::File(Poco::Path(dir).append(autoNames[4]).toString()).exists());
  EXPECT_TRUE(Poco::File(manualName).exists());

  Poco::File(dir).remove(true);
}

TEST(BackupRotationServiceTest, NonExistentDirectoryIsNotAnError) {
  pcm::backup::BackupRotationService service;
  const auto result =
      service.prune("tmp_rotation_dir_missing", "PsyClientManager-auto-", 3);
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.removed_count, 0);
}
```

Add `#include "backup_rotation_service.h"` to the includes block at the top of `test/backup_tests.cpp` (alongside the existing `#include "backup_service.h"` etc.), and `#include <vector>` if not already present via another header (it is not currently included directly).

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-release --target PsyClientManager_backup_tests --parallel`
Expected: FAIL to compile — `backup_rotation_service.h` does not exist and `pcm::backup::BackupRotationService` is undeclared.

- [ ] **Step 3: Create the header**

`src/backup/backup_rotation_service.h`:

```cpp
#pragma once

#include <string>

namespace pcm::backup {

struct RotationResult {
  bool ok = false;
  std::string error;
  int removed_count = 0;
};

class BackupRotationService {
public:
  RotationResult prune(const std::string &directory,
                       const std::string &filename_prefix,
                       int keep_count);
};

} // namespace pcm::backup
```

- [ ] **Step 4: Implement `prune`**

`src/backup/backup_rotation_service.cpp`:

```cpp
#include "backup_rotation_service.h"

#include <Poco/DirectoryIterator.h>
#include <Poco/Exception.h>
#include <Poco/File.h>
#include <Poco/Path.h>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <vector>

namespace pcm::backup {

RotationResult BackupRotationService::prune(const std::string &directory,
                                            const std::string &filename_prefix,
                                            const int keep_count) {
  try {
    const Poco::File dir(directory);
    if (!dir.exists() || !dir.isDirectory()) {
      return {true, {}, 0};
    }

    std::vector<std::string> matching;
    Poco::DirectoryIterator it(dir);
    const Poco::DirectoryIterator end;
    for (; it != end; ++it) {
      if (!it->isFile()) {
        continue;
      }
      const auto name = Poco::Path(it->path()).getFileName();
      if (name.rfind(filename_prefix, 0) == 0) {
        matching.push_back(it->path());
      }
    }

    // Auto-backup filenames embed a zero-padded timestamp
    // (PsyClientManager-auto-yyyyMMdd-HHmmss.psybackup), so descending
    // lexicographic order is descending chronological order.
    std::sort(matching.begin(), matching.end(), std::greater<>());

    int removed = 0;
    const auto keepFrom = static_cast<std::size_t>(std::max(keep_count, 0));
    for (std::size_t i = keepFrom; i < matching.size(); ++i) {
      Poco::File(matching[i]).remove();
      ++removed;
    }
    return {true, {}, removed};
  } catch (const Poco::Exception &ex) {
    return {false, "failed to prune backups: " + ex.displayText(), 0};
  } catch (const std::exception &ex) {
    return {false, std::string("failed to prune backups: ") + ex.what(), 0};
  }
}

} // namespace pcm::backup
```

- [ ] **Step 5: Register the new files in CMake**

In `src/backup/CMakeLists.txt`, add the two new files to the `add_library` sources list:

```cmake
add_library(${TARGET_NAME} STATIC
        backup_manifest.hpp
        checksum_utils.hpp
        backup_service.h
        backup_service.cpp
        backup_validator.h
        backup_validator.cpp
        restore_service.h
        restore_service.cpp
        backup_rotation_service.h
        backup_rotation_service.cpp
)
```

- [ ] **Step 6: Run test to verify it passes**

Run: `cmake --build build-release --target PsyClientManager_backup_tests --parallel && ./build-release/test/PsyClientManager_backup_tests --gtest_filter=BackupRotationServiceTest.*`
Expected: PASS (2 tests).

- [ ] **Step 7: Commit**

```bash
git add src/backup/backup_rotation_service.h src/backup/backup_rotation_service.cpp src/backup/CMakeLists.txt test/backup_tests.cpp
git commit -m "feat(backup): add BackupRotationService for count-based retention"
```

---

### Task 2: `isAutoBackupDue` pure function

**Files:**
- Create: `src/backup/auto_backup_due.h`
- Create: `src/backup/auto_backup_due.cpp`
- Modify: `src/backup/CMakeLists.txt`
- Test: `test/backup_tests.cpp`

**Interfaces:**
- Produces: `bool pcm::backup::isAutoBackupDue(bool enabled, std::int64_t lastRunAtMs, int intervalDays, std::int64_t nowMs)`. Used by Task 4's `AutoBackupScheduler::isDue()`.

- [ ] **Step 1: Write the failing test**

Append to `test/backup_tests.cpp`:

```cpp
TEST(AutoBackupDueTest, DisabledIsNeverDue) {
  EXPECT_FALSE(pcm::backup::isAutoBackupDue(false, 0, 7, 999'999'999'999));
}

TEST(AutoBackupDueTest, NeverRunIsDueImmediatelyWhenEnabled) {
  EXPECT_TRUE(pcm::backup::isAutoBackupDue(true, 0, 7, 1'000));
}

TEST(AutoBackupDueTest, NotYetDueWithinInterval) {
  const std::int64_t lastRun = 1'700'000'000'000;
  const std::int64_t sixDaysLater = lastRun + 6LL * 24 * 60 * 60 * 1000;
  EXPECT_FALSE(pcm::backup::isAutoBackupDue(true, lastRun, 7, sixDaysLater));
}

TEST(AutoBackupDueTest, DueOnceIntervalElapsed) {
  const std::int64_t lastRun = 1'700'000'000'000;
  const std::int64_t sevenDaysLater = lastRun + 7LL * 24 * 60 * 60 * 1000;
  EXPECT_TRUE(pcm::backup::isAutoBackupDue(true, lastRun, 7, sevenDaysLater));
}
```

Add `#include "auto_backup_due.h"` to `test/backup_tests.cpp`'s includes block.

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build-release --target PsyClientManager_backup_tests --parallel`
Expected: FAIL to compile — `auto_backup_due.h` does not exist.

- [ ] **Step 3: Create the header**

`src/backup/auto_backup_due.h`:

```cpp
#pragma once

#include <cstdint>

namespace pcm::backup {

bool isAutoBackupDue(bool enabled, std::int64_t lastRunAtMs, int intervalDays,
                     std::int64_t nowMs);

} // namespace pcm::backup
```

- [ ] **Step 4: Implement it**

`src/backup/auto_backup_due.cpp`:

```cpp
#include "auto_backup_due.h"

namespace pcm::backup {

bool isAutoBackupDue(const bool enabled, const std::int64_t lastRunAtMs,
                     const int intervalDays, const std::int64_t nowMs) {
  if (!enabled) {
    return false;
  }
  if (lastRunAtMs <= 0) {
    return true;
  }
  const std::int64_t intervalMs =
      static_cast<std::int64_t>(intervalDays) * 24 * 60 * 60 * 1000;
  return (nowMs - lastRunAtMs) >= intervalMs;
}

} // namespace pcm::backup
```

- [ ] **Step 5: Register the new files in CMake**

In `src/backup/CMakeLists.txt`, add to the same `add_library` sources list from Task 1:

```cmake
        auto_backup_due.h
        auto_backup_due.cpp
```

- [ ] **Step 6: Run test to verify it passes**

Run: `cmake --build build-release --target PsyClientManager_backup_tests --parallel && ./build-release/test/PsyClientManager_backup_tests --gtest_filter=AutoBackupDueTest.*`
Expected: PASS (4 tests).

- [ ] **Step 7: Run the full backup test suite as a regression check**

Run: `./build-release/test/PsyClientManager_backup_tests`
Expected: all tests pass (existing + 6 new).

- [ ] **Step 8: Commit**

```bash
git add src/backup/auto_backup_due.h src/backup/auto_backup_due.cpp src/backup/CMakeLists.txt test/backup_tests.cpp
git commit -m "feat(backup): add isAutoBackupDue pure due-check function"
```

---

### Task 3: `app_settings` additions

**Files:**
- Modify: `src/widgets/app_settings.h`
- Modify: `src/widgets/app_settings.cpp`

**Interfaces:**
- Produces: `bool autoBackupEnabled()/setAutoBackupEnabled(bool)`, `int autoBackupIntervalDays()/setAutoBackupIntervalDays(int)`, `int autoBackupKeepCount()/setAutoBackupKeepCount(int)`, `QString autoBackupDestination()/setAutoBackupDestination(const QString&)`, `qint64 autoBackupLastRunAtMs()/setAutoBackupLastRunAtMs(qint64)` — all in `pcm::app_settings`. Used by Task 4 (`AutoBackupScheduler`) and Task 5 (Settings UI).

No test for this task: `app_settings` has no existing test coverage anywhere in the codebase (it's a thin `QSettings` wrapper with no logic beyond default values), and this task doesn't change that convention — verified by build success only, consistent with every other setting already in this file.

- [ ] **Step 1: Declare the new functions**

In `src/widgets/app_settings.h`, after the existing `attachmentsStorageRoot()` declaration (last line before the closing `}` of the namespace):

```cpp
QString attachmentsStorageRoot();

bool autoBackupEnabled();
void setAutoBackupEnabled(bool enabled);
int autoBackupIntervalDays();
void setAutoBackupIntervalDays(int days);
int autoBackupKeepCount();
void setAutoBackupKeepCount(int count);
QString autoBackupDestination();
void setAutoBackupDestination(const QString &path);
qint64 autoBackupLastRunAtMs();
void setAutoBackupLastRunAtMs(qint64 ms);
```

- [ ] **Step 2: Add the settings keys and defaults**

In `src/widgets/app_settings.cpp`, in the anonymous namespace at the top, after `kCurrencyCodeKey`:

```cpp
constexpr auto kCurrencyCodeKey = "ui/currency";
constexpr auto kAutoBackupEnabledKey = "backup/autoEnabled";
constexpr auto kAutoBackupIntervalDaysKey = "backup/autoIntervalDays";
constexpr auto kAutoBackupKeepCountKey = "backup/autoKeepCount";
constexpr auto kAutoBackupDestinationKey = "backup/autoDestination";
constexpr auto kAutoBackupLastRunAtMsKey = "backup/autoLastRunAtMs";
```

And after `defaultMeetingInviteTemplateValue()`'s closing brace, still inside the anonymous namespace:

```cpp
int defaultAutoBackupIntervalDaysValue() {
  return 7;
}

int defaultAutoBackupKeepCountValue() {
  return 7;
}

QString defaultAutoBackupDestinationValue() {
  const auto basePath =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  return QDir(basePath).filePath("backups");
}
```

- [ ] **Step 3: Implement the getters/setters**

In `src/widgets/app_settings.cpp`, after `attachmentsStorageRoot()`'s closing brace, still inside `namespace pcm::app_settings`:

```cpp
QString attachmentsStorageRoot() {
  const auto basePath =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  return QDir(basePath).filePath("storage/notes");
}

bool autoBackupEnabled() {
  QSettings settings;
  return settings.value(kAutoBackupEnabledKey, true).toBool();
}

void setAutoBackupEnabled(const bool enabled) {
  QSettings settings;
  settings.setValue(kAutoBackupEnabledKey, enabled);
}

int autoBackupIntervalDays() {
  QSettings settings;
  return settings
      .value(kAutoBackupIntervalDaysKey, defaultAutoBackupIntervalDaysValue())
      .toInt();
}

void setAutoBackupIntervalDays(const int days) {
  QSettings settings;
  settings.setValue(kAutoBackupIntervalDaysKey, days);
}

int autoBackupKeepCount() {
  QSettings settings;
  return settings.value(kAutoBackupKeepCountKey, defaultAutoBackupKeepCountValue())
      .toInt();
}

void setAutoBackupKeepCount(const int count) {
  QSettings settings;
  settings.setValue(kAutoBackupKeepCountKey, count);
}

QString autoBackupDestination() {
  QSettings settings;
  return settings
      .value(kAutoBackupDestinationKey, defaultAutoBackupDestinationValue())
      .toString();
}

void setAutoBackupDestination(const QString &path) {
  QSettings settings;
  settings.setValue(kAutoBackupDestinationKey, path);
}

qint64 autoBackupLastRunAtMs() {
  QSettings settings;
  return settings.value(kAutoBackupLastRunAtMsKey, static_cast<qint64>(0))
      .toLongLong();
}

void setAutoBackupLastRunAtMs(const qint64 ms) {
  QSettings settings;
  settings.setValue(kAutoBackupLastRunAtMsKey, ms);
}

} // namespace pcm::app_settings
```

(The closing `} // namespace pcm::app_settings` moves from directly after the old `attachmentsStorageRoot()` to after the new `setAutoBackupLastRunAtMs()`.)

- [ ] **Step 4: Build**

Run: `cmake --build build-release --target PsyClientManager_widgets --parallel`
Expected: builds cleanly.

- [ ] **Step 5: Commit**

```bash
git add src/widgets/app_settings.h src/widgets/app_settings.cpp
git commit -m "feat(settings): add automatic-backup configuration keys"
```

---

### Task 4: `AutoBackupScheduler` and `Application` wiring

**Files:**
- Create: `src/app/auto_backup_scheduler.h`
- Create: `src/app/auto_backup_scheduler.cpp`
- Modify: `src/app/CMakeLists.txt`
- Modify: `src/app/application.h`
- Modify: `src/app/application.cpp`

**Interfaces:**
- Consumes: `pcm::backup::BackupRotationService::prune` (Task 1), `pcm::backup::isAutoBackupDue` (Task 2), `pcm::app_settings::autoBackup*` (Task 3), `pcm::backup::BackupService::create_backup` / `BackupOptions` (existing).
- Produces: `pcm::backup::AutoBackupScheduler` — constructor `(std::shared_ptr<database::Database> db, QObject *parent = nullptr)`, `void start()`, `bool isDue() const`, `void runAsync()`, signal `void backupFinished(bool ok, const QString &error)`. Used by `Application` in this task, and available for Task 5 if needed (not required there).

- [ ] **Step 1: Create the scheduler header**

`src/app/auto_backup_scheduler.h`:

```cpp
#pragma once

#include <QObject>
#include <QTimer>
#include <memory>

#include "database.h"

namespace pcm::backup {

class AutoBackupScheduler : public QObject {
  Q_OBJECT

public:
  explicit AutoBackupScheduler(std::shared_ptr<database::Database> db,
                               QObject *parent = nullptr);

  void start();
  bool isDue() const;
  void runAsync();

signals:
  void backupFinished(bool ok, const QString &error);

private:
  std::shared_ptr<database::Database> mDb;
  QTimer mTimer;
  bool mRunInProgress = false;
};

} // namespace pcm::backup
```

- [ ] **Step 2: Implement the scheduler**

`src/app/auto_backup_scheduler.cpp`:

```cpp
#include "auto_backup_scheduler.h"

#include "../backup/auto_backup_due.h"
#include "../backup/backup_rotation_service.h"
#include "../backup/backup_service.h"
#include "../widgets/app_settings.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QThread>

namespace pcm::backup {
namespace {

constexpr int kAutoBackupTimerIntervalMs = 60 * 60 * 1000;

class AutoBackupWorker final : public QObject {
  Q_OBJECT

public:
  AutoBackupWorker(std::shared_ptr<database::Database> db, QString destinationPath,
                   QString attachmentsRoot)
      : mDb(std::move(db)), mDestinationPath(std::move(destinationPath)),
        mAttachmentsRoot(std::move(attachmentsRoot)) {}

public slots:
  void run() {
    BackupService service;
    BackupOptions options;
    options.attachments_root = mAttachmentsRoot.toStdString();
    const auto result =
        service.create_backup(*mDb, mDestinationPath.toStdString(), options);
    emit finished(result.ok, QString::fromStdString(result.error));
  }

signals:
  void finished(bool ok, const QString &error);

private:
  std::shared_ptr<database::Database> mDb;
  QString mDestinationPath;
  QString mAttachmentsRoot;
};

} // namespace

AutoBackupScheduler::AutoBackupScheduler(std::shared_ptr<database::Database> db,
                                         QObject *parent)
    : QObject(parent), mDb(std::move(db)) {}

void AutoBackupScheduler::start() {
  mTimer.setInterval(kAutoBackupTimerIntervalMs);
  connect(&mTimer, &QTimer::timeout, this, &AutoBackupScheduler::runAsync);
  mTimer.start();
  runAsync();
}

bool AutoBackupScheduler::isDue() const {
  return isAutoBackupDue(app_settings::autoBackupEnabled(),
                         app_settings::autoBackupLastRunAtMs(),
                         app_settings::autoBackupIntervalDays(),
                         QDateTime::currentMSecsSinceEpoch());
}

void AutoBackupScheduler::runAsync() {
  if (mRunInProgress || !isDue()) {
    return;
  }
  mRunInProgress = true;

  const auto destinationDir = app_settings::autoBackupDestination();
  QDir().mkpath(destinationDir);
  const auto destinationPath =
      QDir(destinationDir)
          .filePath(QStringLiteral("PsyClientManager-auto-%1.psybackup")
                        .arg(QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss")));

  auto *thread = new QThread(this);
  auto *worker = new AutoBackupWorker(mDb, destinationPath,
                                      app_settings::attachmentsStorageRoot());
  worker->moveToThread(thread);

  connect(thread, &QThread::started, worker, &AutoBackupWorker::run);
  connect(worker, &AutoBackupWorker::finished, this,
          [this, destinationDir](const bool ok, const QString &error) {
            mRunInProgress = false;
            if (ok) {
              app_settings::setAutoBackupLastRunAtMs(
                  QDateTime::currentMSecsSinceEpoch());
              BackupRotationService rotation;
              const auto rotationResult =
                  rotation.prune(destinationDir.toStdString(),
                                "PsyClientManager-auto-",
                                app_settings::autoBackupKeepCount());
              if (!rotationResult.ok) {
                qWarning() << "AutoBackupScheduler: rotation failed:"
                           << QString::fromStdString(rotationResult.error);
              }
            } else {
              qWarning() << "AutoBackupScheduler: automatic backup failed:" << error;
            }
            emit backupFinished(ok, error);
          });
  connect(worker, &AutoBackupWorker::finished, thread, &QThread::quit);
  connect(thread, &QThread::finished, worker, &QObject::deleteLater);
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);
  thread->start();
}

} // namespace pcm::backup

#include "auto_backup_scheduler.moc"
```

- [ ] **Step 3: Register the new file in CMake**

In `src/app/CMakeLists.txt`, add `auto_backup_scheduler.cpp` to the `qt_add_library` sources list:

```cmake
qt_add_library(${TARGET_NAME} STATIC
        application.cpp
        main_window.cpp
        settings_dialog.cpp
        auto_backup_scheduler.cpp
)
```

- [ ] **Step 4: Wire the scheduler into `Application`**

In `src/app/application.h`, add the include after `#include "config.h"` (alphabetically grouped with the other local includes):

```cpp
#include "auto_backup_scheduler.h"
#include "config.h"
```

Add the member after `mNotificationTimer`:

```cpp
  QTimer mNotificationTimer;
  std::unique_ptr<pcm::backup::AutoBackupScheduler> mAutoBackupScheduler;
```

In `src/app/application.cpp`, after `mDb = std::make_shared<database::Database>(mConf);` (line 115):

```cpp
  mDb = std::make_shared<database::Database>(mConf);

  mAutoBackupScheduler =
      std::make_unique<pcm::backup::AutoBackupScheduler>(mDb);
  mAutoBackupScheduler->start();
```

Replace `Application::quitApplication()` (lines 295-301):

```cpp
void Application::quitApplication() {
  if (mIsQuitting) {
    return;
  }
  mIsQuitting = true;
  if (mTrayIcon) {
    mTrayIcon->hide();
  }
  if (mAutoBackupScheduler && mAutoBackupScheduler->isDue()) {
    connect(mAutoBackupScheduler.get(),
            &pcm::backup::AutoBackupScheduler::backupFinished, this,
            [](bool, const QString &) { QApplication::quit(); });
    mAutoBackupScheduler->runAsync();
    return;
  }
  QApplication::quit();
}
```

- [ ] **Step 5: Build**

Run: `cmake --build build-release --target PsyClientManager --parallel`
Expected: builds cleanly.

- [ ] **Step 6: Run the full test suite as a regression check**

Run: `ctest --test-dir build-release --output-on-failure`
Expected: all tests pass (no existing test constructs `Application`, so this is a regression guard on the rest of the suite).

- [ ] **Step 7: Commit**

```bash
git add src/app/auto_backup_scheduler.h src/app/auto_backup_scheduler.cpp src/app/CMakeLists.txt src/app/application.h src/app/application.cpp
git commit -m "feat(backup): schedule automatic backups on an interval and on shutdown"
```

---

### Task 5: Settings UI

**Files:**
- Modify: `src/app/settings_dialog.h`
- Modify: `src/app/settings_dialog.cpp`

**Interfaces:**
- Consumes: `pcm::app_settings::autoBackup*` getters/setters (Task 3).

- [ ] **Step 1: Add the new widget members and method declaration**

In `src/app/settings_dialog.h`, add `class QLineEdit;` to the forward-declaration block (alongside the other `class Q...;` lines):

```cpp
class QLabel;
class QLineEdit;
class QProgressBar;
```

Add the new private method after `restoreBackup();`:

```cpp
  void restoreBackup();
  void browseAutoBackupDestination();
```

Add the new member widgets after `mRestoreBackupButton`:

```cpp
  QPushButton *mRestoreBackupButton{nullptr};
  oclero::qlementine::Switch *mAutoBackupEnabledSwitch{nullptr};
  QSpinBox *mAutoBackupIntervalSpinBox{nullptr};
  QSpinBox *mAutoBackupKeepCountSpinBox{nullptr};
  QLineEdit *mAutoBackupDestinationEdit{nullptr};
  QPushButton *mAutoBackupBrowseButton{nullptr};
```

- [ ] **Step 2: Add the include**

In `src/app/settings_dialog.cpp`, add to the includes block (alphabetically, after `<QLabel>`):

```cpp
#include <QLabel>
#include <QLineEdit>
```

- [ ] **Step 3: Build the "Automatic Backups" group box**

In `src/app/settings_dialog.cpp`, insert a new group box right after `generalSettingsLayout->addWidget(backupBox);` (line 240) and before `auto *notificationsBox = new QGroupBox(tr("Notifications"), generalPage);` (line 242):

```cpp
  generalSettingsLayout->addWidget(backupBox);

  auto *autoBackupBox = new QGroupBox(tr("Automatic Backups"), generalPage);
  auto *autoBackupLayout = new QVBoxLayout(autoBackupBox);
  autoBackupLayout->setContentsMargins(16, 16, 16, 16);
  autoBackupLayout->setSpacing(14);
  mAutoBackupEnabledSwitch = new oclero::qlementine::Switch(autoBackupBox);
  mAutoBackupIntervalSpinBox = new QSpinBox(autoBackupBox);
  mAutoBackupIntervalSpinBox->setMinimum(1);
  mAutoBackupIntervalSpinBox->setMaximum(90);
  mAutoBackupIntervalSpinBox->setSuffix(tr(" days"));
  mAutoBackupKeepCountSpinBox = new QSpinBox(autoBackupBox);
  mAutoBackupKeepCountSpinBox->setMinimum(1);
  mAutoBackupKeepCountSpinBox->setMaximum(50);
  auto *destinationRow = new QWidget(autoBackupBox);
  auto *destinationLayout = new QHBoxLayout(destinationRow);
  destinationLayout->setContentsMargins(0, 0, 0, 0);
  destinationLayout->setSpacing(10);
  mAutoBackupDestinationEdit = new QLineEdit(destinationRow);
  mAutoBackupDestinationEdit->setReadOnly(true);
  mAutoBackupBrowseButton = new QPushButton(tr("Browse..."), destinationRow);
  destinationLayout->addWidget(mAutoBackupDestinationEdit, 1);
  destinationLayout->addWidget(mAutoBackupBrowseButton);
  autoBackupLayout->addWidget(makeSettingRow(
      tr("Automatic backups"),
      tr("Periodically create a backup in the background without needing "
        "to click \"Create backup...\"."),
      mAutoBackupEnabledSwitch, autoBackupBox));
  autoBackupLayout->addWidget(
      makeSettingRow(tr("Backup interval"), tr("How often an automatic backup is taken."),
                    mAutoBackupIntervalSpinBox, autoBackupBox));
  autoBackupLayout->addWidget(makeSettingRow(
      tr("Keep last"),
      tr("How many automatic backups to keep before older ones are deleted."),
      mAutoBackupKeepCountSpinBox, autoBackupBox));
  autoBackupLayout->addWidget(makeSettingRow(tr("Destination folder"),
                                             tr("Where automatic backups are saved."),
                                             destinationRow, autoBackupBox));
  generalSettingsLayout->addWidget(autoBackupBox);

  auto *notificationsBox = new QGroupBox(tr("Notifications"), generalPage);
```

- [ ] **Step 4: Load settings into the new widgets**

In `src/app/settings_dialog.cpp`, in `loadSettings()`, after the existing `mNotificationLeadMinutesSpinBox->setEnabled(...)` line (line 375) and before `mPreventOverlapsSwitch->setChecked(...)`:

```cpp
  mNotificationLeadMinutesSpinBox->setEnabled(
      mNotificationsEnabledSwitch->isChecked());
  mAutoBackupEnabledSwitch->setChecked(pcm::app_settings::autoBackupEnabled());
  mAutoBackupIntervalSpinBox->setValue(pcm::app_settings::autoBackupIntervalDays());
  mAutoBackupKeepCountSpinBox->setValue(pcm::app_settings::autoBackupKeepCount());
  mAutoBackupDestinationEdit->setText(pcm::app_settings::autoBackupDestination());
  const auto autoBackupEnabled = mAutoBackupEnabledSwitch->isChecked();
  mAutoBackupIntervalSpinBox->setEnabled(autoBackupEnabled);
  mAutoBackupKeepCountSpinBox->setEnabled(autoBackupEnabled);
  mAutoBackupDestinationEdit->setEnabled(autoBackupEnabled);
  mAutoBackupBrowseButton->setEnabled(autoBackupEnabled);
  mPreventOverlapsSwitch->setChecked(pcm::app_settings::preventEventOverlaps());
```

- [ ] **Step 5: Connect the new widgets' signals**

In `src/app/settings_dialog.cpp`, in `connectSignals()`, after the existing block connecting `mNotificationLeadMinutesSpinBox` (lines 416-419) and before `mPreventOverlapsSwitch`'s connection:

```cpp
  connect(mNotificationLeadMinutesSpinBox, &QSpinBox::valueChanged, this,
          [](const int minutes) {
            pcm::app_settings::setNotificationLeadMinutes(minutes);
          });
  connect(mAutoBackupEnabledSwitch, &QAbstractButton::toggled, this,
          [this](const bool checked) {
            pcm::app_settings::setAutoBackupEnabled(checked);
            mAutoBackupIntervalSpinBox->setEnabled(checked);
            mAutoBackupKeepCountSpinBox->setEnabled(checked);
            mAutoBackupDestinationEdit->setEnabled(checked);
            mAutoBackupBrowseButton->setEnabled(checked);
          });
  connect(mAutoBackupIntervalSpinBox, &QSpinBox::valueChanged, this,
          [](const int days) {
            pcm::app_settings::setAutoBackupIntervalDays(days);
          });
  connect(mAutoBackupKeepCountSpinBox, &QSpinBox::valueChanged, this,
          [](const int count) {
            pcm::app_settings::setAutoBackupKeepCount(count);
          });
  connect(mAutoBackupBrowseButton, &QPushButton::clicked, this,
          &SettingsDialog::browseAutoBackupDestination);
  connect(mPreventOverlapsSwitch, &QAbstractButton::toggled, this,
          [](const bool checked) {
            pcm::app_settings::setPreventEventOverlaps(checked);
          });
```

- [ ] **Step 6: Implement `browseAutoBackupDestination`**

In `src/app/settings_dialog.cpp`, after `SettingsDialog::createBackup()`'s closing brace (after line 514) and before `SettingsDialog::validateBackup()`:

```cpp
void SettingsDialog::browseAutoBackupDestination() {
  const auto selected = QFileDialog::getExistingDirectory(
      this, tr("Select Automatic Backup Folder"),
      mAutoBackupDestinationEdit->text());
  if (selected.isEmpty()) {
    return;
  }
  mAutoBackupDestinationEdit->setText(selected);
  pcm::app_settings::setAutoBackupDestination(selected);
}
```

- [ ] **Step 7: Build**

Run: `cmake --build build-release --target PsyClientManager --parallel`
Expected: builds cleanly.

- [ ] **Step 8: Run the full test suite as a regression check**

Run: `ctest --test-dir build-release --output-on-failure`
Expected: all tests pass.

- [ ] **Step 9: Commit**

```bash
git add src/app/settings_dialog.h src/app/settings_dialog.cpp
git commit -m "feat(settings): add Automatic Backups configuration UI"
```

---

### Task 6: Version bump and CHANGELOG

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/app/application.cpp`
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Bump the version**

In `CMakeLists.txt`:

```cmake
project(PsyClientManager VERSION 0.1.16 LANGUAGES CXX)
```

In `src/app/application.cpp`:

```cpp
  app.setApplicationVersion("0.1.16");
```

- [ ] **Step 2: Add the CHANGELOG entry**

In `CHANGELOG.md`, add above the `[0.1.15]` entry:

```markdown
## [0.1.16] - 2026-07-29

### Added

- Automatic backups: a configurable interval-based background backup with
  count-based retention, plus a shutdown safety-net check, on top of the
  existing manual backup flow. Configurable in Settings → Backup →
  Automatic Backups.
```

- [ ] **Step 3: Build and run full test suite**

Run: `cmake --build build-release --parallel && ctest --test-dir build-release --output-on-failure`
Expected: builds cleanly, all tests pass.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt src/app/application.cpp CHANGELOG.md
git commit -m "chore: bump version to 0.1.16"
```
