# Wire the Backup Service into the UI — Design

## Goal

`BackupService`/`BackupValidator` (`src/backup/`) exist and are fully tested but
are not reachable from the application — nothing in `src/app`/`src/pages`
references `pcm::backup`. This adds two actions to the existing Settings
dialog: create a full `.psybackup` backup, and validate an existing one.

Out of scope: restore, encryption, cloud upload (all already out of scope for
the backup service itself), and any new automated UI tests (see Testing
below).

## Shared Attachments Path

`ClientNotesPage::attachmentsStorageRoot()` (private, in
`src/pages/client_notes_page/client_notes_page.cpp`) currently computes:

```cpp
QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
    .filePath("storage/notes")
```

This is the one thing a full backup and the existing attachment-saving code
must agree on exactly — a second, independently-written copy of this path
would risk a backup silently excluding all attachments if the two ever
drifted.

Extract it into a new free function in the existing shared Qt-facing
settings module:

```cpp
// src/widgets/app_settings.h
namespace pcm::app_settings {
QString attachmentsStorageRoot();
}
```

`ClientNotesPage::attachmentsStorageRoot()` and
`ClientNotesPage::relativeNoteAttachmentPath()`'s caller in
`persistPendingAttachments`/`addAttachmentWidgets` switch to calling
`pcm::app_settings::attachmentsStorageRoot()` instead of the private method,
which is then removed. `src/pages/client_notes_page/CMakeLists.txt` gains a
link to `PsyClientManager_widgets` (currently not linked; `widgets` has no
dependencies of its own, so this introduces no cycle).

## Database Wiring

`MainWindow` already declares (but never sets or reads)
`std::shared_ptr<pcm::database::Database> mDb{nullptr};` — comment: "Database
connection (currently unused)".

- Add `void MainWindow::setDatabase(std::shared_ptr<pcm::database::Database> db);`
  (public), storing into the existing `mDb` member. Remove the "(currently
  unused)" comment.
- `Application::run()` calls `mMainWindow->setDatabase(mDb);` once, alongside
  its existing `mMainWindow->addClientNotesPage(mDb)` /
  `addAnalyticsPage(mDb)` / `addClientCardPage(mDb)` calls.
- `MainWindow::openSettingsDialog()` passes `mDb` into `SettingsDialog`'s
  constructor.

This reuses the shared `Database` instance the rest of the app already has
open — never a second `duckdb::DuckDB` instance pointing at the same file
(which DuckDB's file locking wouldn't permit concurrently in-process anyway).

## `SettingsDialog` Changes

Constructor gains a `std::shared_ptr<pcm::database::Database> db` parameter,
stored as a new private member `mDb`.

A new "Backup" `QGroupBox` is added to `generalSettingsLayout`, directly
after the existing `databaseBox` (Database directory / Open folder), inside
`SettingsDialog::setupUi()`:

- A short description label.
- `mCreateBackupButton` ("Create backup...").
- `mValidateBackupButton` ("Validate backup...").
- `mBackupProgressBar` (`QProgressBar`, indeterminate — `setRange(0, 0)` —
  hidden unless an operation is running).
- `mBackupStatusLabel` (hidden unless an operation is running; text set to
  "Creating backup…" / "Validating backup…").

### Create backup

1. `QFileDialog::getSaveFileName(this, tr("Create Backup"), defaultPath,
   tr("PsyClientManager Backup (*.psybackup)"))`, where `defaultPath` is
   `<Documents>/PsyClientManager-<yyyy-MM-dd_HHmmss>.psybackup`
   (`QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)`).
   Empty result (user canceled) → do nothing.
2. Disable `mCreateBackupButton` and `mValidateBackupButton`, show the
   progress bar and status label.
3. Run `pcm::backup::BackupService{}.create_backup(*mDb, path.toStdString(),
   {pcm::app_settings::attachmentsStorageRoot().toStdString()})` on a
   worker thread (see Threading). Always includes attachments — no toggle;
   a "backup" means the full workspace.
4. On completion (back on the UI thread): hide progress/status, re-enable
   both buttons, then `QMessageBox::information` with the destination path
   on `result.ok`, or `QMessageBox::warning` with `result.error` otherwise.

### Validate backup

1. `QFileDialog::getOpenFileName(this, tr("Validate Backup"), defaultDir,
   tr("PsyClientManager Backup (*.psybackup)"))`, `defaultDir` = Documents.
   Empty result → do nothing.
2. Same disable/progress/status treatment as above (status: "Validating
   backup…").
3. Run `pcm::backup::BackupValidator{}.validate(path.toStdString())` on a
   worker thread.
4. On completion: hide progress/status, re-enable both buttons, then
   `QMessageBox::information` if `result.ok`, or `QMessageBox::warning`
   listing `result.errors` (newline-joined; if there are more than 10,
   show the first 10 followed by `"... and N more"`) otherwise.

Both buttons are disabled while either operation is running — one worker at
a time, no queuing.

## Threading

Follows the existing pattern from `QClientModel`/`ClientLoaderWorker`
(`src/client_model/qclient_model.cpp`): a `QObject`-derived worker in an
anonymous namespace, moved to a `QThread`, started via a `Qt::QueuedConnection`
signal, reporting back via a plain `connect` (Qt marshals it onto the UI
thread automatically since the receiver lives there). Unlike
`ClientLoaderWorker` (one persistent thread for the model's lifetime), the
backup/validate worker and its thread are created fresh per operation and
torn down (`quit()` + `deleteLater()` on both) when it finishes, since backup
actions are rare, one-shot operations rather than a live data source.

Two worker classes (`BackupWorker`, running `create_backup`;
`ValidateWorker`, running `validate`) or one worker with two slots — left to
the implementer's judgment during planning, whichever keeps
`settings_dialog.cpp` clearest; both must end up private to that file's
anonymous namespace, matching `ClientLoaderWorker`'s existing precedent.

## Error Handling

`BackupResult` (`ok`/`error`) and `ValidationResult` (`ok`/`errors`) already
carry everything the UI needs. The UI layer does no interpretation of
*why* something failed beyond displaying the message(s) — it only handles
its own concerns: empty file-dialog results, button/progress state, and
which dialog (information vs. warning) to show.

## Testing

This change is Qt widget/threading glue over an already fully-tested
`BackupService`/`BackupValidator` — no new business logic. Per
`docs/asciidoc/07-testing.adoc`, this project's GoogleTest suite covers
headless modules only (`config`, `database`, `event_view`, `backup`);
UI/page interaction flows are explicitly not covered anywhere in `src/app`
or `src/pages`. Consistent with that existing scope boundary, this change
adds no new automated tests. Verification is manual: build and run the app,
open Settings → Database → Backup, create a backup and confirm the
resulting file and dialog, validate it (expect success), then validate a
deliberately corrupted or non-`.psybackup` file (expect a clear failure
message).

## File Changes Summary

- Modify: `src/widgets/app_settings.h`, `src/widgets/app_settings.cpp` (add
  `attachmentsStorageRoot()`)
- Modify: `src/pages/client_notes_page/client_notes_page.h/.cpp` (remove
  private `attachmentsStorageRoot()`, call the shared one)
- Modify: `src/pages/client_notes_page/CMakeLists.txt` (link
  `PsyClientManager_widgets`)
- Modify: `src/app/main_window.h/.cpp` (`setDatabase`, use `mDb` in
  `openSettingsDialog`)
- Modify: `src/app/application.cpp` (call `setDatabase`)
- Modify: `src/app/settings_dialog.h/.cpp` (constructor param, new UI group,
  two worker classes, wiring)
- Modify: `src/app/CMakeLists.txt` (link `PsyClientManager_backup`)
- Modify: `CMakeLists.txt` (version bump), `CHANGELOG.md`, `README.md` (per
  this repo's contributor workflow — every MR raises the version and updates
  the changelog)
