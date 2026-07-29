# Automatic Backups with Retention Policy — Design

Issue: #25 — "Add automatic backups with retention/rotation policy"

## Goal

Reduce reliance on the user remembering to click "Create backup..." by adding
scheduled automatic backups with a retention policy, on top of the manual
`.psybackup` flow already shipped (`BackupService`, `RestoreService`).

## Scope (this pass)

- **Triggers:** an interval-based check ("back up if the last automatic
  backup is older than N days"), driven by a periodic timer while the app is
  running, plus the same due-check run once more on app shutdown as a safety
  net (not an unconditional extra backup on every close).
- **`BackupRotationService`:** prunes old automatic backups by copy count.
- **Settings UI:** enable/disable, interval (days), keep-count, destination
  folder.

## Out of scope

- Cloud backup destinations (roadmap 0.5.0).
- Encrypted backups (tracked separately, issues #33/#34).
- Pre-migration automatic backups (age/count retention only for this pass;
  a schema-migration-triggered backup is a separate future trigger, not
  needed to satisfy this issue's acceptance criteria, which only mention the
  configured trigger + retention + failure-safety).

## Architecture

Two new components in `src/backup/`, following the existing separation of
`BackupService`/`RestoreService`/`BackupValidator` into single-purpose
classes:

- **`BackupRotationService`** — pure file-system pruning logic, no Qt
  dependency, easily unit-testable.
- **`AutoBackupScheduler`** — a `QObject` owned by `Application`, mirroring
  the existing `mNotificationTimer`/`checkUpcomingEventNotifications()`
  pattern already used for reminder polling. Owns the timer, the "is a
  backup due" check, and orchestrates `BackupService::create_backup` on a
  background `QThread` (reusing the shape of `SettingsDialog`'s existing
  `BackupWorker`), followed by rotation and persisting the last-run
  timestamp.

Keeping this as a dedicated scheduler class (rather than extending
`Application` directly) keeps `application.cpp` from growing further and
lets the due-check/rotation logic be unit-tested without a full
`Application`/tray-icon instance.

## Components

### `BackupRotationService`

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

`prune()` lists files in `directory` whose filename starts with
`filename_prefix` and ends in `.psybackup`, sorts them by filename
descending (auto-backup filenames embed a zero-padded timestamp, so
lexicographic order is chronological order), keeps the newest
`keep_count`, and deletes the rest. Returns the count removed. A
non-existent directory or zero matching files is not an error (returns
`{true, "", 0}`).

### Filename convention

Automatic backups are named `PsyClientManager-auto-yyyyMMdd-hhmmss.psybackup`
— distinct from manual backups (`PsyClientManager-yyyyMMdd...`, no `-auto-`
segment) so rotation never touches a manually-created file even if it lives
in the same folder.

### `app_settings` additions

`src/widgets/app_settings.h`/`.cpp` gain, following the existing free-function
+ `QSettings` key pattern:

```cpp
bool autoBackupEnabled();
void setAutoBackupEnabled(bool enabled);
int autoBackupIntervalDays();
void setAutoBackupIntervalDays(int days);
int autoBackupKeepCount();
void setAutoBackupKeepCount(int count);
QString autoBackupDestination();
void setAutoBackupDestination(const QString &path);

// Not user-facing; internal bookkeeping only.
qint64 autoBackupLastRunAtMs();
void setAutoBackupLastRunAtMs(qint64 ms);
```

Defaults: `autoBackupEnabled` = `true`, `autoBackupIntervalDays` = `7`,
`autoBackupKeepCount` = `7`, `autoBackupDestination` =
`QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)` +
`/backups` (same base-path pattern `attachmentsStorageRoot()` already uses),
`autoBackupLastRunAtMs` = `0` (never run).

### `AutoBackupScheduler`

`src/backup/auto_backup_scheduler.h`:

```cpp
#pragma once

#include <QObject>
#include <QTimer>
#include <memory>

#include "database.h"

namespace pcm::backup {

// Pure function, no Qt/settings/filesystem I/O — unit-testable directly.
bool isAutoBackupDue(bool enabled, qint64 lastRunAtMs, int intervalDays,
                     qint64 nowMs);

class AutoBackupScheduler : public QObject {
  Q_OBJECT

public:
  explicit AutoBackupScheduler(
      std::shared_ptr<database::Database> db, QObject *parent = nullptr);

  void start();
  bool isDue() const;
  void runAsync();

signals:
  void backupFinished(bool ok, const QString &error);

private:
  std::shared_ptr<database::Database> mDb;
  QTimer mTimer;
};

} // namespace pcm::backup
```

`isAutoBackupDue()` is a free function — `enabled && (nowMs - lastRunAtMs)
>= intervalDays * 86'400'000LL` — kept separate from any `app_settings`/Qt
I/O so it can be unit-tested with fabricated inputs, without touching the
user's real `QSettings` store. `AutoBackupScheduler::isDue()` is a thin
wrapper: reads `autoBackupEnabled`/`autoBackupLastRunAtMs`/
`autoBackupIntervalDays` from `app_settings` and `QDateTime::
currentMSecsSinceEpoch()`, and calls the free function. `runAsync()` is a
no-op if `!isDue()`; otherwise it builds `autoBackupDestination()` + the
timestamped filename, runs `BackupService::create_backup` on a background
`QThread`, and on completion — success or failure — calls
`BackupRotationService::prune` (only on success, so a failed backup doesn't
prune away good ones), updates `autoBackupLastRunAtMs` (only on success, so
a failure retries next tick), and emits `backupFinished`.

## Data Flow

1. `Application::run()` constructs `mAutoBackupScheduler` after `mDb` is
   built, calls `mAutoBackupScheduler->start()`.
2. `start()` arms an hourly `QTimer` (`connect(&mTimer, &QTimer::timeout,
   this, &AutoBackupScheduler::runAsync)`) and calls `runAsync()` once
   immediately, mirroring how `checkUpcomingEventNotifications()` is both
   timer-driven and called once at startup.
3. On shutdown, `Application::quitApplication()` calls
   `mAutoBackupScheduler->isDue()`; if true, it connects
   `backupFinished` to a lambda that calls `QApplication::quit()`, calls
   `runAsync()`, and returns without quitting yet — the backup completes
   on its background thread before the app actually exits. If not due, it
   quits immediately as today.

## Settings UI

New "Automatic Backups" `QGroupBox` in `SettingsDialog`'s existing Backup
page, below the manual create/validate/restore buttons: an enable
checkbox, an interval `QSpinBox` (days, range 1–90), a keep-count
`QSpinBox` (range 1–50), and a destination row (a read-only `QLineEdit` +
"Browse..." button using `QFileDialog::getExistingDirectory`, mirroring the
existing `QFileDialog::getSaveFileName` usage for manual backups). Changes
write straight through to `app_settings` on edit, matching how every other
setting in this dialog already behaves (no separate "Save" step).

## Error Handling

- A failed `create_backup` call: logged via `PLOG_ERROR`, does not touch
  any existing backup file (the underlying service already writes to a
  `.partial-<uuid>` temp file and renames atomically on success only), does
  not update `autoBackupLastRunAtMs` (so the next timer tick retries), and
  never shows a blocking dialog to the user — automatic backups are
  silent-success/silent-retry by design, satisfying the AC that a failed
  automatic backup must not crash the app or corrupt existing backups.
- A failed `BackupRotationService::prune` (e.g. permission error deleting an
  old file): logged, does not fail the backup itself (the new backup was
  already written successfully) and does not block `autoBackupLastRunAtMs`
  from updating.

## Testing

- `BackupRotationServiceTest`: create `N + k` fake `.psybackup` files with
  distinct timestamped names in a temp directory, call `prune(dir, prefix,
  N)`, assert exactly the `N` newest remain and `removed_count == k`.
  Also test the empty-directory and no-matching-files cases return
  `{true, "", 0}`.
- `isAutoBackupDue()` gets a direct unit test with fabricated
  `enabled`/`lastRunAtMs`/`intervalDays`/`nowMs` inputs covering: disabled
  (never due regardless of age), never-run (`lastRunAtMs == 0`, due
  immediately when enabled), just-under-interval (not due), and
  just-over-interval (due). No `QSettings`/`QApplication` touched — the
  user's real settings store is never written to by tests.
