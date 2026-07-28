# Wire Restore Into the UI — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a "Restore backup..." action to Settings that lets a user pick a `.psybackup` file, pre-validates it, and stages a deferred restore that's applied automatically the next time the app launches (before the database connection opens), per `docs/superpowers/specs/2026-07-28-restore-ui-design.md`.

**Architecture:** `RestoreService` (`src/backup`) gains three free functions for a JSON "pending restore" marker file. `SettingsDialog` reuses the existing `ValidateWorker` to pre-check the chosen backup on a worker thread, then writes the marker and quits the app on confirmation. `Application::run()` is restructured so `mDb` is constructed after `QApplication` exists; right before that construction, it checks for a pending-restore marker, applies the restore via `RestoreService::restore_backup()`, shows a result `QMessageBox`, and deletes the marker — guaranteeing the swap always runs in a fresh process with no open database handle.

**Tech Stack:** C++20, Qt6 Widgets, Poco (File/Path), reflect-cpp (`rfl::json`), GoogleTest.

## Global Constraints

- Restore must never run while the live `duckdb::DuckDB` connection is open — the marker/next-launch model is mandatory, no in-session restore path.
- The pending-restore marker lives at `<configHome>/PsyClientManager/pending-restore.json` — a sibling of `Config.yaml`, `database/`, and `storage/`, so it survives the directory swap it triggers.
- `rfl::Skip<Poco::Path>` (e.g. `Config::config_pth`) requires `.value()` to access the underlying `Poco::Path`.
- No new automated UI tests — matches the existing convention that `src/app`/`src/pages` have zero test coverage in this repo. Only the new `RestoreService` marker functions get unit tests.
- Raise the application version for this MR: `CMakeLists.txt` and `src/app/application.cpp` both go from `0.1.12` to `0.1.13`.
- Update `CHANGELOG.md` with the user-visible change.
- Two-space indentation, existing Qt naming conventions (`PascalCase` classes, `camelCase` methods).

---

### Task 1: `RestoreService` pending-restore marker functions

**Files:**
- Modify: `src/backup/restore_service.h`
- Modify: `src/backup/restore_service.cpp`
- Test: `test/backup_tests.cpp`

**Interfaces:**
- Produces: `struct pcm::backup::PendingRestoreMarker { std::string backup_path; };`
- Produces: `bool pcm::backup::write_pending_restore_marker(const std::string &marker_path, const std::string &backup_path);`
- Produces: `std::optional<pcm::backup::PendingRestoreMarker> pcm::backup::read_pending_restore_marker(const std::string &marker_path);`
- Produces: `void pcm::backup::remove_pending_restore_marker(const std::string &marker_path);`
- Consumes: `rfl::json::save`/`rfl::json::load` (already used the same way for `BackupManifest` in `backup_service.cpp:126-133`), `Poco::File`.

- [ ] **Step 1: Write the failing tests**

Append to `test/backup_tests.cpp` (after the last `RestoreServiceTest` test, so near the end of the file):

```cpp
TEST(PendingRestoreMarkerTest, WriteThenReadRoundTripsBackupPath) {
  const auto markerPath = Poco::Path(Poco::Path::current())
                              .append("tmp_pending_restore.json")
                              .toString();
  if (Poco::File(markerPath).exists()) {
    Poco::File(markerPath).remove();
  }

  ASSERT_TRUE(pcm::backup::write_pending_restore_marker(
      markerPath, "/backups/example.psybackup"));

  const auto marker = pcm::backup::read_pending_restore_marker(markerPath);
  ASSERT_TRUE(marker.has_value());
  EXPECT_EQ(marker->backup_path, "/backups/example.psybackup");

  Poco::File(markerPath).remove();
}

TEST(PendingRestoreMarkerTest, ReadReturnsNulloptWhenFileIsMissing) {
  const auto markerPath = Poco::Path(Poco::Path::current())
                              .append("tmp_pending_restore_missing.json")
                              .toString();
  if (Poco::File(markerPath).exists()) {
    Poco::File(markerPath).remove();
  }

  EXPECT_FALSE(
      pcm::backup::read_pending_restore_marker(markerPath).has_value());
}

TEST(PendingRestoreMarkerTest, RemoveDeletesTheMarkerFile) {
  const auto markerPath = Poco::Path(Poco::Path::current())
                              .append("tmp_pending_restore_remove.json")
                              .toString();
  ASSERT_TRUE(pcm::backup::write_pending_restore_marker(
      markerPath, "/backups/example.psybackup"));
  ASSERT_TRUE(Poco::File(markerPath).exists());

  pcm::backup::remove_pending_restore_marker(markerPath);

  EXPECT_FALSE(Poco::File(markerPath).exists());
}

TEST(PendingRestoreMarkerTest, RemoveIsANoOpWhenFileIsMissing) {
  const auto markerPath = Poco::Path(Poco::Path::current())
                              .append("tmp_pending_restore_noop.json")
                              .toString();
  if (Poco::File(markerPath).exists()) {
    Poco::File(markerPath).remove();
  }

  EXPECT_NO_THROW(pcm::backup::remove_pending_restore_marker(markerPath));
}
```

- [ ] **Step 2: Configure the build with tests enabled and confirm the new tests fail to compile**

Run: `cmake --preset vcpkg-release -DPCM_BUILD_TESTS=ON && cmake --build build-release --target PsyClientManager_backup_tests --parallel`
Expected: FAIL — compile error, `write_pending_restore_marker`/`read_pending_restore_marker`/`remove_pending_restore_marker` are not declared.

- [ ] **Step 3: Add the marker declarations to the header**

In `src/backup/restore_service.h`, after the `RestoreOptions`/`RestoreResult` structs and before `class RestoreService`, add:

```cpp
struct PendingRestoreMarker {
  std::string backup_path;
};

bool write_pending_restore_marker(const std::string &marker_path,
                                   const std::string &backup_path);
std::optional<PendingRestoreMarker>
read_pending_restore_marker(const std::string &marker_path);
void remove_pending_restore_marker(const std::string &marker_path);
```

The file's full header contents become:

```cpp
#pragma once

#include <optional>
#include <string>

namespace pcm::backup {

struct RestoreOptions {
  std::optional<std::string> attachments_root;
};

struct RestoreResult {
  bool ok = false;
  std::string error;
  std::string protective_database_path;
  std::string protective_attachments_path;
};

struct PendingRestoreMarker {
  std::string backup_path;
};

bool write_pending_restore_marker(const std::string &marker_path,
                                   const std::string &backup_path);
std::optional<PendingRestoreMarker>
read_pending_restore_marker(const std::string &marker_path);
void remove_pending_restore_marker(const std::string &marker_path);

class RestoreService {
public:
  RestoreResult restore_backup(const std::string &backup_path,
                               const std::string &database_root,
                               const RestoreOptions &options = {});
};

} // namespace pcm::backup
```

- [ ] **Step 4: Implement the marker functions**

In `src/backup/restore_service.cpp`, add the implementations after the closing brace of the anonymous namespace (`} // namespace`, currently line 110) and before `RestoreResult RestoreService::restore_backup(...)`:

```cpp
bool write_pending_restore_marker(const std::string &marker_path,
                                  const std::string &backup_path) {
  PendingRestoreMarker marker;
  marker.backup_path = backup_path;
  const auto saveResult =
      rfl::json::save(marker_path, marker, rfl::json::pretty);
  return static_cast<bool>(saveResult);
}

std::optional<PendingRestoreMarker>
read_pending_restore_marker(const std::string &marker_path) {
  if (!Poco::File(marker_path).exists()) {
    return std::nullopt;
  }
  const auto parsed = rfl::json::load<PendingRestoreMarker>(marker_path);
  if (!parsed) {
    return std::nullopt;
  }
  return parsed.value();
}

void remove_pending_restore_marker(const std::string &marker_path) {
  try {
    Poco::File file(marker_path);
    if (file.exists()) {
      file.remove();
    }
  } catch (...) {
  }
}
```

No new includes are needed — `restore_service.cpp` already includes `<Poco/File.h>`, `<optional>` (via the header), and `<rfl/json.hpp>`.

- [ ] **Step 5: Build and run the new tests**

Run: `cmake --build build-release --target PsyClientManager_backup_tests --parallel && ./build-release/test/PsyClientManager_backup_tests --gtest_filter=PendingRestoreMarkerTest.*`
Expected: PASS — all 4 `PendingRestoreMarkerTest` cases green.

- [ ] **Step 6: Run the full backup test suite to confirm no regressions**

Run: `./build-release/test/PsyClientManager_backup_tests`
Expected: PASS — all tests green (previous `RestoreServiceTest`/`BackupServiceTest`/etc. plus the 4 new ones).

- [ ] **Step 7: Commit**

```bash
git add src/backup/restore_service.h src/backup/restore_service.cpp test/backup_tests.cpp
git commit -m "feat(backup): add pending-restore marker read/write/remove"
```

---

### Task 2: Apply pending restore at app bootstrap

**Files:**
- Modify: `src/app/application.h`
- Modify: `src/app/application.cpp`

**Interfaces:**
- Consumes: `pcm::backup::read_pending_restore_marker`, `pcm::backup::remove_pending_restore_marker`, `pcm::backup::RestoreService::restore_backup`, `pcm::backup::RestoreOptions` (Task 1); `pcm::app_settings::attachmentsStorageRoot()` (`src/widgets/app_settings.h`, existing); `pcm::config::Config::config_pth`/`db_conf.value_.db_pth` (`src/config/config.h`, existing).
- Produces: no new symbols — `Application`'s public interface (`Application()`, `run(argc, argv)`) is unchanged; only its internal sequencing changes.

- [ ] **Step 1: Move `mDb` construction out of the constructor**

In `src/app/application.cpp`, change:

```cpp
Application::Application() {
  mDb = std::make_shared<database::Database>(mConf);
}
```

to:

```cpp
Application::Application() = default;
```

- [ ] **Step 2: Add the includes needed for the pending-restore check**

In `src/app/application.cpp`, add to the include block (after `#include "../widgets/app_settings.h"`):

```cpp
#include "../backup/restore_service.h"

#include <Poco/Path.h>
#include <QMessageBox>
```

- [ ] **Step 3: Insert the pending-restore check/apply/notify step and construct `mDb`**

In `src/app/application.cpp`, `Application::run()` currently goes straight from the translation-loading block into:

```cpp
  mMainWindow = std::make_unique<MainWindow>();
  mClientModel = std::make_shared<QClientModel>(mDb);
```

`mClientModel` needs `mDb`, so insert the new block immediately before `mMainWindow = std::make_unique<MainWindow>();` (i.e. after translations are loaded, so restore result messages are shown in the right language, and before anything that depends on `mDb`):

```cpp
  const auto markerPath = Poco::Path(mConf.config_pth.value())
                              .makeParent()
                              .append("pending-restore.json")
                              .toString();
  if (const auto marker = pcm::backup::read_pending_restore_marker(markerPath)) {
    pcm::backup::RestoreService service;
    pcm::backup::RestoreOptions options;
    options.attachments_root =
        pcm::app_settings::attachmentsStorageRoot().toStdString();
    const auto result = service.restore_backup(
        marker->backup_path, mConf.db_conf.value_.db_pth.toString(), options);
    pcm::backup::remove_pending_restore_marker(markerPath);
    if (result.ok) {
      QMessageBox::information(nullptr, tr("Restore Complete"),
                               tr("The backup was restored successfully."));
    } else {
      QMessageBox::warning(
          nullptr, tr("Restore Failed"),
          tr("The restore could not be completed:\n%1\n\nYour previous "
             "data was kept.").arg(QString::fromStdString(result.error)));
    }
  }

  mDb = std::make_shared<database::Database>(mConf);

  mMainWindow = std::make_unique<MainWindow>();
  mClientModel = std::make_shared<QClientModel>(mDb);
```

- [ ] **Step 4: Confirm `Application::Application() = default;` still matches its header declaration**

Open `src/app/application.h` and confirm line 24 still reads `Application();` (a plain declaration is compatible with an out-of-line `= default;` definition — no header change needed). No edit required; this step is a verification checkpoint before building.

- [ ] **Step 5: Build the app target**

Run: `cmake --build build-release --target PsyClientManager --parallel`
Expected: builds cleanly with no errors.

- [ ] **Step 6: Manually verify normal startup is unaffected**

Run: `./build-release/PsyClientManager` (adjust path if the packaged binary lives elsewhere in `build-release/`)
Expected: the app launches exactly as before (no pending marker exists yet, so the `if` block is skipped and `mDb` is constructed immediately after).

- [ ] **Step 7: Commit**

```bash
git add src/app/application.h src/app/application.cpp
git commit -m "feat(app): apply a pending restore at startup, before the database opens"
```

---

### Task 3: "Restore backup..." action in Settings

**Files:**
- Modify: `src/app/settings_dialog.h`
- Modify: `src/app/settings_dialog.cpp`

**Interfaces:**
- Consumes: `ValidateWorker` (anonymous namespace, `src/app/settings_dialog.cpp:92-116`, unchanged) — `ValidateWorker(QString backupPath)`, slot `run()`, signal `finished(bool ok, const QStringList &errors)`; `pcm::backup::write_pending_restore_marker` (Task 1); `mConfig.config_pth` (`pcm::config::Config`, existing member).
- Produces: `SettingsDialog::restoreBackup()` (private slot-like method, same pattern as `createBackup()`/`validateBackup()`); no new public interface.

- [ ] **Step 1: Add the button and method declarations to the header**

In `src/app/settings_dialog.h`, change:

```cpp
  void createBackup();
  void validateBackup();
```

to:

```cpp
  void createBackup();
  void validateBackup();
  void restoreBackup();
```

and change:

```cpp
  QPushButton *mCreateBackupButton{nullptr};
  QPushButton *mValidateBackupButton{nullptr};
```

to:

```cpp
  QPushButton *mCreateBackupButton{nullptr};
  QPushButton *mValidateBackupButton{nullptr};
  QPushButton *mRestoreBackupButton{nullptr};
```

- [ ] **Step 2: Add the includes needed for the restore flow**

In `src/app/settings_dialog.cpp`, add to the include block (after `#include "backup_validator.h"`):

```cpp
#include "restore_service.h"
```

and add to the Qt include block (after `#include <QApplication>` doesn't exist yet — add it alphabetically before `#include <QComboBox>`):

```cpp
#include <QApplication>
```

and add to the Poco include (there is none yet in this file — add after the last `#include <Q...>` line, before `namespace {`):

```cpp
#include <Poco/Path.h>
```

- [ ] **Step 3: Add the button to the Backup group box UI**

In `src/app/settings_dialog.cpp`, `setupUi()`, change:

```cpp
  mCreateBackupButton = new QPushButton(tr("Create backup..."), backupBox);
  mValidateBackupButton = new QPushButton(tr("Validate backup..."), backupBox);
  backupButtonsLayout->addWidget(mCreateBackupButton);
  backupButtonsLayout->addWidget(mValidateBackupButton);
  backupButtonsLayout->addStretch();
```

to:

```cpp
  mCreateBackupButton = new QPushButton(tr("Create backup..."), backupBox);
  mValidateBackupButton = new QPushButton(tr("Validate backup..."), backupBox);
  mRestoreBackupButton = new QPushButton(tr("Restore backup..."), backupBox);
  backupButtonsLayout->addWidget(mCreateBackupButton);
  backupButtonsLayout->addWidget(mValidateBackupButton);
  backupButtonsLayout->addWidget(mRestoreBackupButton);
  backupButtonsLayout->addStretch();
```

- [ ] **Step 4: Wire the button's signal**

In `src/app/settings_dialog.cpp`, `connectSignals()`, change:

```cpp
  connect(mValidateBackupButton, &QPushButton::clicked, this,
          &SettingsDialog::validateBackup);
```

to:

```cpp
  connect(mValidateBackupButton, &QPushButton::clicked, this,
          &SettingsDialog::validateBackup);
  connect(mRestoreBackupButton, &QPushButton::clicked, this,
          &SettingsDialog::restoreBackup);
```

- [ ] **Step 5: Implement `restoreBackup()`**

In `src/app/settings_dialog.cpp`, add after `SettingsDialog::validateBackup()` (after its closing brace, before `#include "settings_dialog.moc"`):

```cpp
void SettingsDialog::restoreBackup() {
  const auto defaultDir =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  const auto backupPath = QFileDialog::getOpenFileName(
      this, tr("Restore Backup"), defaultDir,
      tr("PsyClientManager Backup (*.psybackup)"));
  if (backupPath.isEmpty()) {
    return;
  }

  mCreateBackupButton->setEnabled(false);
  mValidateBackupButton->setEnabled(false);
  mRestoreBackupButton->setEnabled(false);
  mBackupStatusLabel->setText(tr("Checking backup…"));
  mBackupStatusLabel->setVisible(true);
  mBackupProgressBar->setVisible(true);

  auto *thread = new QThread(this);
  auto *worker = new ValidateWorker(backupPath);
  worker->moveToThread(thread);

  connect(thread, &QThread::started, worker, &ValidateWorker::run);
  connect(worker, &ValidateWorker::finished, this,
          [this, backupPath](const bool ok, const QStringList &errors) {
            mBackupStatusLabel->setVisible(false);
            mBackupProgressBar->setVisible(false);
            mCreateBackupButton->setEnabled(true);
            mValidateBackupButton->setEnabled(true);
            mRestoreBackupButton->setEnabled(true);
            if (!ok) {
              QString message;
              if (errors.size() > 10) {
                message = errors.mid(0, 10).join('\n') +
                          tr("\n... and %1 more").arg(errors.size() - 10);
              } else {
                message = errors.join('\n');
              }
              QMessageBox::warning(this, tr("Backup Invalid"), message);
              return;
            }

            const auto confirmation = QMessageBox::warning(
                this, tr("Restore Backup"),
                tr("This will replace all current data (clients, events, "
                   "notes, and attachments) with the contents of this "
                   "backup. Your current data will be kept as a protective "
                   "copy, but the application must restart to complete the "
                   "restore. Continue?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (confirmation != QMessageBox::Yes) {
              return;
            }

            const auto markerPath = Poco::Path(mConfig.config_pth.value())
                                        .makeParent()
                                        .append("pending-restore.json")
                                        .toString();
            if (!pcm::backup::write_pending_restore_marker(
                    markerPath, backupPath.toStdString())) {
              QMessageBox::warning(
                  this, tr("Restore Failed"),
                  tr("Could not stage the restore. Check that there is "
                     "enough disk space and try again."));
              return;
            }

            QMessageBox::information(
                this, tr("Restore Staged"),
                tr("PsyClientManager will now close. Restart it to "
                   "complete the restore."));
            QApplication::quit();
          });
  connect(worker, &ValidateWorker::finished, thread, &QThread::quit);
  connect(thread, &QThread::finished, worker, &QObject::deleteLater);
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);
  thread->start();
}
```

- [ ] **Step 6: Add the backup module's include path so `restore_service.h` resolves**

Check `src/app/CMakeLists.txt` already links `${PROJECT_NAME}_backup` (added in the earlier backup-UI work) — run `grep -n "_backup" src/app/CMakeLists.txt` to confirm both `add_dependencies` and `target_link_libraries` list it. Since `restore_service.h` lives in the same `src/backup` include directory as `backup_service.h`/`backup_validator.h` (already `#include`d successfully in this file), no CMake changes are expected. If the grep shows it's missing, add it following the exact pattern already used for `backup_service.h`.

- [ ] **Step 7: Build the app target**

Run: `cmake --build build-release --target PsyClientManager --parallel`
Expected: builds cleanly with no errors.

- [ ] **Step 8: Manual verification**

Run: `./build-release/PsyClientManager`
1. Open Settings → click "Create backup..." → save a `.psybackup` file somewhere.
2. Click "Restore backup..." → select a file that is *not* a valid backup (e.g. any other file, rename extension if needed) → confirm "Backup Invalid" appears and no restart is required.
3. Click "Restore backup..." → select the `.psybackup` created in step 1 → confirm the "Restore Backup" warning dialog appears → click **No** → confirm nothing else happens.
4. Click "Restore backup..." again → select the same file → click **Yes** → confirm the "Restore Staged" message appears and the application quits.
5. Relaunch the app → confirm a "Restore Complete" message appears on startup and the app opens normally afterward.

Expected: all 5 steps behave as described.

- [ ] **Step 9: Commit**

```bash
git add src/app/settings_dialog.h src/app/settings_dialog.cpp
git commit -m "feat(settings): add Restore backup... action"
```

---

### Task 4: Version bump, changelog, and README

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/app/application.cpp`
- Modify: `CHANGELOG.md`
- Modify: `README.md`

**Interfaces:** none (metadata-only changes).

- [ ] **Step 1: Bump the project version**

In `CMakeLists.txt`, change:

```cmake
project(PsyClientManager VERSION 0.1.12 LANGUAGES CXX)
```

to:

```cmake
project(PsyClientManager VERSION 0.1.13 LANGUAGES CXX)
```

- [ ] **Step 2: Bump the application version string**

In `src/app/application.cpp`, change:

```cpp
  app.setApplicationVersion("0.1.12");
```

to:

```cpp
  app.setApplicationVersion("0.1.13");
```

- [ ] **Step 3: Add the changelog entry**

In `CHANGELOG.md`, insert a new section above `## [0.1.12] - 2026-07-26`:

```markdown
## [0.1.13] - 2026-07-28

### Added

- "Restore backup..." action in Settings, using a deferred apply-on-restart
  flow so the restore only runs with no open database connection.
```

- [ ] **Step 4: Update the README feature line**

In `README.md`, change:

```markdown
- Staged restore from `.psybackup` with protective pre-restore copies
```

to:

```markdown
- Staged restore from `.psybackup`, triggered from Settings, with
  protective pre-restore copies and an apply-on-restart flow
```

- [ ] **Step 5: Reconfigure so CMake picks up the new project version**

Run: `cmake -S . -B build-release`
Expected: reconfigures without errors; `CPACK_PACKAGE_VERSION`/`PROJECT_VERSION` now read `0.1.13`.

- [ ] **Step 6: Full rebuild and full test suite**

Run: `cmake --build build-release --parallel && ctest --test-dir build-release --output-on-failure`
Expected: all targets build, all tests pass (including the 4 new `PendingRestoreMarkerTest` cases from Task 1).

- [ ] **Step 7: Commit**

```bash
git add CMakeLists.txt src/app/application.cpp CHANGELOG.md README.md
git commit -m "chore: bump version to 0.1.13"
```
