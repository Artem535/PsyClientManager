# Wire Restore Into the UI — Design

## Goal

`RestoreService::restore_backup()` (`src/backup/restore_service.h`, merged via
#18) exists and is fully tested but isn't reachable from the application.
This adds a "Restore backup..." action to Settings, using a
deferred-apply-on-next-launch model so the restore's file swap only ever
runs in a fresh process with no open database connection.

Out of scope: encrypted backups, cloud storage (already out of scope for the
backup/restore services themselves), and any new automated UI tests (see
Testing below, matching the backup-UI precedent).

## Why Deferred, Not Immediate

`RestoreService::restore_backup()` renames the live database/attachments
directories out of the way and stages new ones in. `docs/asciidoc/05-modules.adoc`
already documents the constraint this imposes: *"The application must close
active database connections before replacement."* This app keeps one
`duckdb::DuckDB` connection open for its entire process lifetime, shared via
`std::shared_ptr<database::Database>` across `MainWindow` and every page —
there's no way to close it without tearing down the whole UI.

Rather than trying to closes-and-reopen the live connection mid-session
(complex, and calling `restore_backup()` while a handle to the target
directory is still open risks a sharing-violation failure on Windows),
restore is staged from the running app and **applied at the very start of
the next launch**, before `Database` is ever constructed in that fresh
process — guaranteeing no open handle exists at swap time, on any platform.

## Directory Layout (why the marker lives where it does)

```
<configHome>/PsyClientManager/
├── Config.yaml                  (pcm::config::Config)
├── database/                    (db_pth — entirely replaced by restore)
│   ├── database.db
│   └── database.log
└── storage/notes/                (attachmentsStorageRoot() — entirely replaced when the backup has attachments)
```

The pending-restore marker must live at the `PsyClientManager/` level —
a sibling of `database/` and `storage/`, never inside either — so it
survives being read back after both of those directories get renamed away
and replaced.

## `RestoreService` Additions

Add to `src/backup/restore_service.h`/`.cpp` (kept config-agnostic like the
rest of `src/backup` — callers pass paths in, nothing here knows about
`pcm::config::Config` or Qt):

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

- `write_pending_restore_marker`: JSON via `rfl::json::save` (same style as
  `BackupManifest`), just `{"backup_path": "..."}`. The database root and
  attachments root are never stored in the marker — both are recomputed
  fresh from `pcm::config::Config`/`pcm::app_settings::attachmentsStorageRoot()`
  at apply time, since they're deterministic and storing them risks staleness
  if settings change between staging and the next launch.
- `read_pending_restore_marker`: `rfl::json::load`; returns `std::nullopt` if
  the file doesn't exist or fails to parse (a corrupt/missing marker should
  never block normal startup — treat it the same as "no pending restore").
- `remove_pending_restore_marker`: best-effort delete, swallows errors (same
  `try { } catch (...) { }` pattern as `ScratchGuard`) — called unconditionally
  after an apply attempt, success or failure, so a failed restore doesn't
  retry forever on every subsequent launch.

## Bootstrap Sequencing (`src/app`)

Today, `main()` is `return pcm::Application().run(argc, argv);`, and
`Application::Application()`'s constructor immediately does
`mDb = std::make_shared<database::Database>(mConf);` — meaning the database
opens before `run()` even starts, before any `QApplication` exists in that
call chain. Showing a result `QMessageBox` requires a `QApplication` to
exist first, so:

- **Move DB construction out of `Application::Application()`** and into the
  top of `Application::run()`, after `QApplication app(argc, argv);` is
  constructed. `Application`'s constructor becomes trivial (no `mDb` setup).
- At the top of `run()`, right after constructing `QApplication app`, and
  right before constructing `mDb`:
  ```cpp
  const auto markerPath = Poco::Path(mConf.config_pth.value()).makeParent()
                               .append("pending-restore.json").toString();
  if (const auto marker = pcm::backup::read_pending_restore_marker(markerPath)) {
    pcm::backup::RestoreService service;
    pcm::backup::RestoreOptions options;
    options.attachments_root =
        pcm::app_settings::attachmentsStorageRoot().toStdString();
    const auto result = service.restore_backup(
        marker->backup_path,
        mConf.db_conf.value_.db_pth.toString(), options);
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
  ```
- Everything else in `run()` (translations, `MainWindow` construction, page
  wiring) proceeds unchanged after this, now opening whatever ended up on
  disk — the freshly restored data on success, or the untouched original on
  failure (guaranteed by `RestoreService`'s own rollback).

## `SettingsDialog` Changes

A third button, `mRestoreBackupButton` ("Restore backup..."), added to the
existing Backup `QGroupBox`, after `mValidateBackupButton`.

Flow:

1. `QFileDialog::getOpenFileName` (same filter/default-dir as Validate)
   picks a `.psybackup` file. Empty result → do nothing.
2. Disable all three backup buttons, show progress/status ("Checking
   backup…") — same treatment as Create/Validate.
3. Run a **pre-check** on a worker thread, reusing the existing
   `ValidateWorker` class as-is (no new worker type) — this is a fast-fail
   step so a corrupt or wrong file is rejected immediately, without asking
   the user to restart the app just to find out.
4. On pre-check failure: hide progress/status, re-enable buttons, show the
   same "Backup Invalid" `QMessageBox::warning` the Validate flow already
   uses (error list, capped at 10 + "N more").
5. On pre-check success: hide progress/status, re-enable buttons, then show
   a confirmation `QMessageBox::warning` (Yes/No, default **No**):
   > "Restore Backup — This will replace all current data (clients,
   > events, notes, and attachments) with the contents of this backup. Your
   > current data will be kept as a protective copy, but the application
   > must restart to complete the restore. Continue?"
6. On **No** (or dialog dismissed): do nothing further.
7. On **Yes**: call `pcm::backup::write_pending_restore_marker(markerPath,
   selectedPath.toStdString())`. If it fails (e.g. disk full), show a
   `QMessageBox::warning` ("Could not stage the restore: ...") and stop —
   nothing has been touched. If it succeeds, show
   `QMessageBox::information` ("Restore staged. PsyClientManager will now
   close — restart it to complete the restore."), then quit the application
   (`QApplication::quit()` — same as the tray "Quit" action already wires
   to, bypassing the tray-minimize-on-close behavior in
   `Application::eventFilter`).

`SettingsDialog` needs the marker path too — computed the same way as in
`run()` (`Config.yaml`'s parent directory), added as a private helper method
`markerPath()` or a small free function shared between `settings_dialog.cpp`
and `application.cpp`. Given it's a two-line Poco::Path computation used in
exactly two places, duplicating it inline in both call sites (matching this
codebase's general tolerance for small, obvious duplication over premature
sharing) is acceptable; if it grows a third use site later, extract it then.

## Error Handling

Same principle as the existing Create/Validate actions: the UI layer does no
interpretation of *why* something failed beyond displaying the message — it
only handles empty file-dialog results, button/progress state, and which
dialog to show. `RestoreResult`/pre-check `ValidationResult` already carry
everything needed.

## Testing

No new automated tests for the UI wiring itself — matches the established
convention (`src/app`/`src/pages` have no test coverage anywhere in this
repo). The `RestoreService` backend it calls is already covered by its own
test suite from #18.

Manual verification:
1. Create a backup, then click "Restore backup...", select it, confirm the
   pre-check passes and the warning dialog appears.
2. Confirm restore, verify the app shows "Restore staged" and quits.
3. Relaunch the app, verify a "Restore Complete" message appears and the
   data matches the backup.
4. Click "Restore backup..." with a non-`.psybackup`/corrupted file, verify
   it's rejected via "Backup Invalid" without requiring a restart.
5. (If feasible) simulate a next-launch failure (e.g. by making the staged
   marker point at a file that's deleted between staging and relaunch) and
   confirm the "Restore Failed" message appears and existing data is intact.

## File Changes Summary

- Modify: `src/backup/restore_service.h`, `src/backup/restore_service.cpp`
  (add `PendingRestoreMarker` + the three marker functions)
- Modify: `src/app/application.h`, `src/app/application.cpp` (move DB
  construction into `run()`, add the pending-restore check/apply/notify step)
- Modify: `src/app/settings_dialog.h`, `src/app/settings_dialog.cpp`
  (`mRestoreBackupButton`, restore flow reusing `ValidateWorker`)
- Modify: `CMakeLists.txt` (version bump), `CHANGELOG.md`, `README.md` (per
  this repo's contributor workflow)
