# Wire Backup Service Into Settings UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add "Create backup..." and "Validate backup..." actions to the Settings dialog, backed by the already-implemented `pcm::backup::BackupService`/`BackupValidator`.

**Architecture:** Extract the shared attachments-storage path into `pcm::app_settings`; wire `MainWindow`'s already-declared-but-unused `Database` pointer through to `SettingsDialog`; add a "Backup" group with two buttons, each running its backup-module call on a worker `QThread` (same pattern as `QClientModel`/`ClientLoaderWorker`), reporting a result dialog back on the UI thread.

**Tech Stack:** C++20, Qt6 Widgets, the existing `pcm::backup` module (`BackupService`, `BackupValidator`, `BackupOptions`, `BackupResult`, `ValidationResult`).

Spec: `docs/superpowers/specs/2026-07-26-backup-service-ui-design.md`

## Global Constraints

- Backups are always full (database + attachments) — no UI toggle for database-only.
- Attachments root must come from exactly one place: the new `pcm::app_settings::attachmentsStorageRoot()`, used by both `ClientNotesPage` and the new backup action — never a second independently-computed copy of that path.
- `BackupService`/`BackupValidator` calls run off the UI thread, following the existing `QObject`-worker-moved-to-`QThread` pattern from `src/client_model/qclient_model.cpp` (`ClientLoaderWorker`) — not `QtConcurrent` (not linked in this project) and not a blocking direct call.
- Any `QObject`-derived class defined entirely inside a `.cpp` file needs `#include "<file>.moc"` as the last line of that `.cpp` file (see `qclient_model.cpp`'s ending) for AUTOMOC to generate its meta-object code — omitting it is a link error (`undefined reference to vtable for ...`), not a compile error, so it's easy to miss.
- No new automated tests: this project's GoogleTest suite covers headless modules only (`config`, `database`, `backup`, `event_view`); `src/app`/`src/pages` have no test coverage today, and this change doesn't add any either. Verification is: full build succeeds, and (for the final task) a manual run-through.
- Every MR raises the version in both `CMakeLists.txt` and `src/app/application.cpp`, and updates `CHANGELOG.md` (per `AGENTS.md`).

---

### Task 1: Extract shared attachments-storage-root helper

**Files:**
- Modify: `src/widgets/app_settings.h`
- Modify: `src/widgets/app_settings.cpp`
- Modify: `src/pages/client_notes_page/client_notes_page.h`
- Modify: `src/pages/client_notes_page/client_notes_page.cpp`
- Modify: `src/pages/client_notes_page/CMakeLists.txt`

**Interfaces:**
- Produces: `QString pcm::app_settings::attachmentsStorageRoot();` — later tasks (Task 3) call this exact function to build `BackupOptions::attachments_root`.

- [ ] **Step 1: Add the function declaration**

In `src/widgets/app_settings.h`, add this line inside `namespace pcm::app_settings { ... }`, right after the existing `QString languageCode(); void setLanguageCode(...)` pair (anywhere inside the namespace block is fine — this keeps it near the other `QString`-returning path/setting helpers):

```cpp
QString attachmentsStorageRoot();
```

- [ ] **Step 2: Implement it**

In `src/widgets/app_settings.cpp`, add `#include <QDir>` and `#include <QStandardPaths>` to the top include block (alongside the existing `#include <QColor>` / `#include <QObject>` / `#include <QSettings>` / `#include <QTime>`), then add this function inside `namespace pcm::app_settings { ... }` (anywhere in the namespace body, e.g. right before the closing `} // namespace pcm::app_settings`):

```cpp
QString attachmentsStorageRoot() {
  const auto basePath =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  return QDir(basePath).filePath("storage/notes");
}
```

This is byte-for-byte the same computation `ClientNotesPage::attachmentsStorageRoot()` currently does (see Step 3).

- [ ] **Step 3: Point `ClientNotesPage` at the shared helper**

In `src/pages/client_notes_page/client_notes_page.h`, remove this line (currently in the `private:` section, among the other private methods):

```cpp
  [[nodiscard]] QString attachmentsStorageRoot() const;
```

In `src/pages/client_notes_page/client_notes_page.cpp`:

1. Add `#include "../../widgets/app_settings.h"` to the top include block (matching the existing relative-include style already used in this file for `#include "../../widgets/constants.hpp"`).
2. Remove the method definition:
   ```cpp
   QString ClientNotesPage::attachmentsStorageRoot() const {
     const auto basePath =
         QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
     return QDir(basePath).filePath("storage/notes");
   }
   ```
3. Replace its two call sites — change:
   ```cpp
   const auto absolutePath =
       QDir(attachmentsStorageRoot()).filePath(relativePath);
   ```
   to:
   ```cpp
   const auto absolutePath =
       QDir(pcm::app_settings::attachmentsStorageRoot()).filePath(relativePath);
   ```
   and change:
   ```cpp
   const auto rootPath = attachmentsStorageRoot();
   ```
   to:
   ```cpp
   const auto rootPath = pcm::app_settings::attachmentsStorageRoot();
   ```

- [ ] **Step 4: Link `client_notes_page` against `widgets`**

In `src/pages/client_notes_page/CMakeLists.txt`, replace:

```cmake
target_link_libraries(${TARGET_NAME} PUBLIC
        ${PROJECT_NAME}_database
        Qt6::Widgets
)
```

with:

```cmake
target_link_libraries(${TARGET_NAME} PUBLIC
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_widgets
        Qt6::Widgets
)
```

Also add `${PROJECT_NAME}_widgets` to the `add_dependencies(...)` call right above it (currently only lists `${PROJECT_NAME}_database`).

- [ ] **Step 5: Build to verify**

Run: `cmake --build build --target PsyClientManager_app`
Expected: builds with no errors (this also builds `client_notes_page` and `widgets` as dependencies). If `build/` isn't configured yet, run `cmake -S . -B build -DPCM_BUILD_TESTS=ON` first.

- [ ] **Step 6: Commit**

```bash
git add src/widgets/app_settings.h src/widgets/app_settings.cpp src/pages/client_notes_page/client_notes_page.h src/pages/client_notes_page/client_notes_page.cpp src/pages/client_notes_page/CMakeLists.txt
git commit -m "refactor(app): share attachments storage root between notes and backup"
```

---

### Task 2: Wire the shared `Database` through to `SettingsDialog`

**Files:**
- Modify: `src/app/main_window.h`
- Modify: `src/app/main_window.cpp`
- Modify: `src/app/application.cpp`
- Modify: `src/app/settings_dialog.h`
- Modify: `src/app/settings_dialog.cpp`

**Interfaces:**
- Consumes: nothing from Task 1.
- Produces: `SettingsDialog`'s constructor becomes `explicit SettingsDialog(std::shared_ptr<pcm::database::Database> db, QWidget *parent = nullptr);` and gains a private member `std::shared_ptr<pcm::database::Database> mDb;` — Task 3 uses this member directly.

- [ ] **Step 1: Add `MainWindow::setDatabase`**

In `src/app/main_window.h`, replace:

```cpp
  void addClientNotesPage(std::shared_ptr<pcm::database::Database> db);
```

with:

```cpp
  void addClientNotesPage(std::shared_ptr<pcm::database::Database> db);
  void setDatabase(std::shared_ptr<pcm::database::Database> db);
```

Then replace the comment above the existing `mDb` member — change:

```cpp
  // Database connection (currently unused)
  std::shared_ptr<pcm::database::Database> mDb{nullptr};
```

to:

```cpp
  // Database connection, used by SettingsDialog for backup/validate actions.
  std::shared_ptr<pcm::database::Database> mDb{nullptr};
```

- [ ] **Step 2: Implement `setDatabase` and use it in `openSettingsDialog`**

In `src/app/main_window.cpp`, add this method definition near the other `addXPage` method definitions (e.g. right after `MainWindow::addClientNotesPage(...)`'s closing brace):

```cpp
void MainWindow::setDatabase(std::shared_ptr<pcm::database::Database> db) {
  mDb = std::move(db);
}
```

Then replace:

```cpp
void MainWindow::openSettingsDialog() {
  SettingsDialog dialog(this);
  dialog.exec();
  refreshPageAppearance();
}
```

with:

```cpp
void MainWindow::openSettingsDialog() {
  SettingsDialog dialog(mDb, this);
  dialog.exec();
  refreshPageAppearance();
}
```

- [ ] **Step 3: Call `setDatabase` from `Application::run()`**

In `src/app/application.cpp`, replace:

```cpp
  mMainWindow->addClientNotesPage(mDb);
  mMainWindow->connectSignals();
```

with:

```cpp
  mMainWindow->addClientNotesPage(mDb);
  mMainWindow->setDatabase(mDb);
  mMainWindow->connectSignals();
```

- [ ] **Step 4: Widen `SettingsDialog`'s constructor**

In `src/app/settings_dialog.h`, add a forward declaration near the top (after the existing `class QDialogButtonBox;` etc. forward declarations, before the `oclero::qlementine` namespace block):

```cpp
namespace pcm::database {
class Database;
}
```

Add `#include <memory>` to the top include block (alongside `#include <QDialog>`).

Replace:

```cpp
  explicit SettingsDialog(QWidget *parent = nullptr);
```

with:

```cpp
  explicit SettingsDialog(std::shared_ptr<pcm::database::Database> db,
                          QWidget *parent = nullptr);
```

Add a new private member at the end of the private member list (after `pcm::config::Config mConfig;`):

```cpp
  std::shared_ptr<pcm::database::Database> mDb;
```

- [ ] **Step 5: Update the constructor definition**

In `src/app/settings_dialog.cpp`, replace:

```cpp
SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent) {
  setupUi();
  loadSettings();
  connectSignals();
}
```

with:

```cpp
SettingsDialog::SettingsDialog(std::shared_ptr<pcm::database::Database> db,
                               QWidget *parent)
    : QDialog(parent), mDb(std::move(db)) {
  setupUi();
  loadSettings();
  connectSignals();
}
```

`mDb` isn't used yet in this task — that's expected, it's consumed starting in Task 3.

- [ ] **Step 6: Build to verify**

Run: `cmake --build build --target PsyClientManager_app`
Expected: builds with no errors. If this fails with "unused private field" or a similar warning treated as an error, that's not expected here (this codebase doesn't build with `-Werror`) — but if it does occur, it means Step 5 was applied without Task 3 following — re-check that `mDb` is a plain member (unused-but-stored members don't warn in this codebase's build config).

- [ ] **Step 7: Commit**

```bash
git add src/app/main_window.h src/app/main_window.cpp src/app/application.cpp src/app/settings_dialog.h src/app/settings_dialog.cpp
git commit -m "feat(app): pass the shared Database into SettingsDialog"
```

---

### Task 3: "Create backup..." action

**Files:**
- Modify: `src/app/CMakeLists.txt`
- Modify: `src/app/settings_dialog.h`
- Modify: `src/app/settings_dialog.cpp`

**Interfaces:**
- Consumes: `mDb` (Task 2, `std::shared_ptr<pcm::database::Database>`), `pcm::app_settings::attachmentsStorageRoot()` (Task 1), `pcm::backup::BackupService::create_backup(const database::Database&, const std::string&, const BackupOptions&)`, `pcm::backup::BackupOptions{ std::optional<std::string> attachments_root; }`, `pcm::backup::BackupResult{ bool ok; std::string error; }` (all pre-existing in `src/backup/backup_service.h`).
- Produces: `SettingsDialog`'s "Backup" `QGroupBox` with `mCreateBackupButton`, `mValidateBackupButton` (button added now, wired in Task 4), `mBackupProgressBar`, `mBackupStatusLabel` — Task 4 adds to this same group and reuses these exact member names.

- [ ] **Step 1: Link the backup module into `src/app`**

In `src/app/CMakeLists.txt`, replace:

```cmake
add_dependencies(${TARGET_NAME}
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_client_page
        ${PROJECT_NAME}_client_info_card_page
        ${PROJECT_NAME}_client_notes_page
        ${PROJECT_NAME}_analytics_page
        ${PROJECT_NAME}_event_page
        ${PROJECT_NAME}_widgets

)

# Link Slint to the module
target_link_libraries(${TARGET_NAME} PUBLIC
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_client_page
        ${PROJECT_NAME}_client_info_card_page
        ${PROJECT_NAME}_client_notes_page
        ${PROJECT_NAME}_analytics_page
        ${PROJECT_NAME}_event_page
        ${PROJECT_NAME}_widgets
        qlementine
        Qt6::Widgets
)
```

with:

```cmake
add_dependencies(${TARGET_NAME}
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_backup
        ${PROJECT_NAME}_client_page
        ${PROJECT_NAME}_client_info_card_page
        ${PROJECT_NAME}_client_notes_page
        ${PROJECT_NAME}_analytics_page
        ${PROJECT_NAME}_event_page
        ${PROJECT_NAME}_widgets

)

# Link Slint to the module
target_link_libraries(${TARGET_NAME} PUBLIC
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_backup
        ${PROJECT_NAME}_client_page
        ${PROJECT_NAME}_client_info_card_page
        ${PROJECT_NAME}_client_notes_page
        ${PROJECT_NAME}_analytics_page
        ${PROJECT_NAME}_event_page
        ${PROJECT_NAME}_widgets
        qlementine
        Qt6::Widgets
)
```

- [ ] **Step 2: Add member declarations and forward declares**

In `src/app/settings_dialog.h`, add `class QProgressBar;` to the forward-declared `class Q...;` block (alphabetically, after `class QPushButton;` — before `class QSpinBox;`).

Replace:

```cpp
  QLabel *mDatabasePathLabel{nullptr};
  QPushButton *mOpenDatabaseFolderButton{nullptr};
```

with:

```cpp
  QLabel *mDatabasePathLabel{nullptr};
  QPushButton *mOpenDatabaseFolderButton{nullptr};
  QPushButton *mCreateBackupButton{nullptr};
  QPushButton *mValidateBackupButton{nullptr};
  QProgressBar *mBackupProgressBar{nullptr};
  QLabel *mBackupStatusLabel{nullptr};
```

Add two new private method declarations, right after `void openDatabaseFolder() const;`:

```cpp
  void createBackup();
  void validateBackup();
```

(`validateBackup()` is declared now so Task 4 only needs to add its body and wiring — its own header edit is done here to keep this task's header diff complete and avoid touching `settings_dialog.h` again in Task 4.)

Note these three new methods are NOT `const` (unlike `openDatabaseFolder`) — they mutate UI state (disabling buttons, starting a thread).

- [ ] **Step 3: Add the "Backup" group to the UI**

In `src/app/settings_dialog.cpp`, add these includes to the top block:

```cpp
#include "backup_service.h"
#include "backup_validator.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressBar>
#include <QStandardPaths>
#include <QThread>
```

(Insert alphabetically among the existing `#include <Q...>` lines; `backup_service.h`/`backup_validator.h` go with the other non-Qt includes near the top, alongside `"../widgets/app_settings.h"` if present — add `#include "../widgets/app_settings.h"` too, since Step 5 of this task calls `pcm::app_settings::attachmentsStorageRoot()`.)

In `SettingsDialog::setupUi()`, replace:

```cpp
  mOpenDatabaseFolderButton = new QPushButton(tr("Open folder"), databaseBox);
  databaseLayout->addWidget(dbPathTitle);
  databaseLayout->addWidget(mDatabasePathLabel);
  databaseLayout->addWidget(mOpenDatabaseFolderButton, 0, Qt::AlignLeft);
  generalSettingsLayout->addWidget(databaseBox);
```

with:

```cpp
  mOpenDatabaseFolderButton = new QPushButton(tr("Open folder"), databaseBox);
  databaseLayout->addWidget(dbPathTitle);
  databaseLayout->addWidget(mDatabasePathLabel);
  databaseLayout->addWidget(mOpenDatabaseFolderButton, 0, Qt::AlignLeft);
  generalSettingsLayout->addWidget(databaseBox);

  auto *backupBox = new QGroupBox(tr("Backup"), generalPage);
  auto *backupLayout = new QVBoxLayout(backupBox);
  backupLayout->setContentsMargins(16, 16, 16, 16);
  backupLayout->setSpacing(10);
  auto *backupDescription = new QLabel(
      tr("Create a full backup (database and attachments) as a single "
        ".psybackup file, or validate an existing one."),
      backupBox);
  backupDescription->setWordWrap(true);
  backupDescription->setStyleSheet("color: rgba(255, 255, 255, 0.68);");
  auto *backupButtonsRow = new QWidget(backupBox);
  auto *backupButtonsLayout = new QHBoxLayout(backupButtonsRow);
  backupButtonsLayout->setContentsMargins(0, 0, 0, 0);
  backupButtonsLayout->setSpacing(10);
  mCreateBackupButton = new QPushButton(tr("Create backup..."), backupBox);
  mValidateBackupButton = new QPushButton(tr("Validate backup..."), backupBox);
  backupButtonsLayout->addWidget(mCreateBackupButton);
  backupButtonsLayout->addWidget(mValidateBackupButton);
  backupButtonsLayout->addStretch();
  mBackupStatusLabel = new QLabel(backupBox);
  mBackupStatusLabel->setStyleSheet("color: rgba(255, 255, 255, 0.68);");
  mBackupStatusLabel->setVisible(false);
  mBackupProgressBar = new QProgressBar(backupBox);
  mBackupProgressBar->setRange(0, 0);
  mBackupProgressBar->setTextVisible(false);
  mBackupProgressBar->setVisible(false);
  backupLayout->addWidget(backupDescription);
  backupLayout->addWidget(backupButtonsRow);
  backupLayout->addWidget(mBackupStatusLabel);
  backupLayout->addWidget(mBackupProgressBar);
  generalSettingsLayout->addWidget(backupBox);
```

- [ ] **Step 4: Wire the "Create backup..." button**

In `SettingsDialog::connectSignals()`, replace:

```cpp
  connect(mOpenDatabaseFolderButton, &QPushButton::clicked, this,
          &SettingsDialog::openDatabaseFolder);
```

with:

```cpp
  connect(mOpenDatabaseFolderButton, &QPushButton::clicked, this,
          &SettingsDialog::openDatabaseFolder);
  connect(mCreateBackupButton, &QPushButton::clicked, this,
          &SettingsDialog::createBackup);
```

- [ ] **Step 5: Implement `createBackup()` and its worker**

Add this anonymous-namespace worker class near the top of `src/app/settings_dialog.cpp`, right after the existing `namespace { QWidget *makeSettingRow(...) { ... } } // namespace` block (as its own second anonymous-namespace block, or inside the same one — either is fine, put it right after `makeSettingRow`'s closing `}` and before the `} // namespace` that closes the file's anonymous namespace):

```cpp
class BackupWorker final : public QObject {
  Q_OBJECT

public:
  BackupWorker(std::shared_ptr<pcm::database::Database> db,
              QString destinationPath, QString attachmentsRoot)
      : mDb(std::move(db)), mDestinationPath(std::move(destinationPath)),
        mAttachmentsRoot(std::move(attachmentsRoot)) {}

public slots:
  void run() {
    pcm::backup::BackupService service;
    pcm::backup::BackupOptions options;
    options.attachments_root = mAttachmentsRoot.toStdString();
    const auto result =
        service.create_backup(*mDb, mDestinationPath.toStdString(), options);
    emit finished(result.ok, QString::fromStdString(result.error));
  }

signals:
  void finished(bool ok, const QString &error);

private:
  std::shared_ptr<pcm::database::Database> mDb;
  QString mDestinationPath;
  QString mAttachmentsRoot;
};
```

Then add the method implementation, anywhere after `SettingsDialog::openDatabaseFolder()`'s definition:

```cpp
void SettingsDialog::createBackup() {
  const auto defaultName =
      QStringLiteral("PsyClientManager-%1.psybackup")
          .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HHmmss"));
  const auto defaultDir =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  const auto destinationPath = QFileDialog::getSaveFileName(
      this, tr("Create Backup"), QDir(defaultDir).filePath(defaultName),
      tr("PsyClientManager Backup (*.psybackup)"));
  if (destinationPath.isEmpty()) {
    return;
  }

  mCreateBackupButton->setEnabled(false);
  mValidateBackupButton->setEnabled(false);
  mBackupStatusLabel->setText(tr("Creating backup…"));
  mBackupStatusLabel->setVisible(true);
  mBackupProgressBar->setVisible(true);

  auto *thread = new QThread(this);
  auto *worker = new BackupWorker(mDb, destinationPath,
                                  pcm::app_settings::attachmentsStorageRoot());
  worker->moveToThread(thread);

  connect(thread, &QThread::started, worker, &BackupWorker::run);
  connect(worker, &BackupWorker::finished, this,
          [this, destinationPath](const bool ok, const QString &error) {
            mBackupStatusLabel->setVisible(false);
            mBackupProgressBar->setVisible(false);
            mCreateBackupButton->setEnabled(true);
            mValidateBackupButton->setEnabled(true);
            if (ok) {
              QMessageBox::information(
                  this, tr("Backup Created"),
                  tr("Backup created at:\n%1").arg(destinationPath));
            } else {
              QMessageBox::warning(this, tr("Backup Failed"), error);
            }
          });
  connect(worker, &BackupWorker::finished, thread, &QThread::quit);
  connect(thread, &QThread::finished, worker, &QObject::deleteLater);
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);
  thread->start();
}
```

Note `QDateTime` and `QDir` are already used elsewhere in this file's includes' transitive closure via other Qt headers in this codebase's usual style, but to be safe add `#include <QDateTime>` and `#include <QDir>` to the include block from Step 3 if they aren't already pulled in — check by building (Step 7); add them explicitly if the build fails with "incomplete type" errors for `QDateTime`/`QDir`.

- [ ] **Step 6: Add the `.moc` include**

At the very end of `src/app/settings_dialog.cpp` (after the last `}` closing `SettingsDialog::openDatabaseFolder()`), add:

```cpp

#include "settings_dialog.moc"
```

- [ ] **Step 7: Build to verify**

Run: `cmake --build build --target PsyClientManager_app`
Expected: builds with no errors. If it fails with an undefined-reference/vtable-related linker error mentioning `BackupWorker`, check Step 6's `.moc` include is present and spelled exactly `settings_dialog.moc` (matching this file's own name).

- [ ] **Step 8: Commit**

```bash
git add src/app/CMakeLists.txt src/app/settings_dialog.h src/app/settings_dialog.cpp
git commit -m "feat(app): add Create backup action to Settings"
```

---

### Task 4: "Validate backup..." action

**Files:**
- Modify: `src/app/settings_dialog.cpp`

**Interfaces:**
- Consumes: `mValidateBackupButton`, `mBackupProgressBar`, `mBackupStatusLabel` (Task 3, already declared and constructed), `pcm::backup::BackupValidator::validate(const std::string&)`, `pcm::backup::ValidationResult{ bool ok; std::vector<std::string> errors; }` (pre-existing in `src/backup/backup_validator.h`).
- Produces: nothing consumed by a later task — this is the last functional task.

- [ ] **Step 1: Wire the "Validate backup..." button**

In `SettingsDialog::connectSignals()`, replace:

```cpp
  connect(mCreateBackupButton, &QPushButton::clicked, this,
          &SettingsDialog::createBackup);
```

with:

```cpp
  connect(mCreateBackupButton, &QPushButton::clicked, this,
          &SettingsDialog::createBackup);
  connect(mValidateBackupButton, &QPushButton::clicked, this,
          &SettingsDialog::validateBackup);
```

- [ ] **Step 2: Add the validate worker**

Add this class in the same anonymous namespace as `BackupWorker` (right after `BackupWorker`'s closing `};`):

```cpp
class ValidateWorker final : public QObject {
  Q_OBJECT

public:
  explicit ValidateWorker(QString backupPath)
      : mBackupPath(std::move(backupPath)) {}

public slots:
  void run() {
    pcm::backup::BackupValidator validator;
    const auto result = validator.validate(mBackupPath.toStdString());
    QStringList errors;
    errors.reserve(static_cast<int>(result.errors.size()));
    for (const auto &error : result.errors) {
      errors << QString::fromStdString(error);
    }
    emit finished(result.ok, errors);
  }

signals:
  void finished(bool ok, const QStringList &errors);

private:
  QString mBackupPath;
};
```

- [ ] **Step 3: Implement `validateBackup()`**

Add this method implementation right after `SettingsDialog::createBackup()`'s closing brace:

```cpp
void SettingsDialog::validateBackup() {
  const auto defaultDir =
      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
  const auto backupPath = QFileDialog::getOpenFileName(
      this, tr("Validate Backup"), defaultDir,
      tr("PsyClientManager Backup (*.psybackup)"));
  if (backupPath.isEmpty()) {
    return;
  }

  mCreateBackupButton->setEnabled(false);
  mValidateBackupButton->setEnabled(false);
  mBackupStatusLabel->setText(tr("Validating backup…"));
  mBackupStatusLabel->setVisible(true);
  mBackupProgressBar->setVisible(true);

  auto *thread = new QThread(this);
  auto *worker = new ValidateWorker(backupPath);
  worker->moveToThread(thread);

  connect(thread, &QThread::started, worker, &ValidateWorker::run);
  connect(worker, &ValidateWorker::finished, this,
          [this](const bool ok, const QStringList &errors) {
            mBackupStatusLabel->setVisible(false);
            mBackupProgressBar->setVisible(false);
            mCreateBackupButton->setEnabled(true);
            mValidateBackupButton->setEnabled(true);
            if (ok) {
              QMessageBox::information(this, tr("Backup Valid"),
                                       tr("The backup is valid."));
            } else {
              QString message;
              if (errors.size() > 10) {
                message = errors.mid(0, 10).join('\n') +
                          tr("\n... and %1 more").arg(errors.size() - 10);
              } else {
                message = errors.join('\n');
              }
              QMessageBox::warning(this, tr("Backup Invalid"), message);
            }
          });
  connect(worker, &ValidateWorker::finished, thread, &QThread::quit);
  connect(thread, &QThread::finished, worker, &QObject::deleteLater);
  connect(thread, &QThread::finished, thread, &QObject::deleteLater);
  thread->start();
}
```

- [ ] **Step 4: Build to verify**

Run: `cmake --build build --target PsyClientManager_app`
Expected: builds with no errors.

- [ ] **Step 5: Full test suite regression check**

Run: `ctest --test-dir build --output-on-failure`
Expected: all existing tests still pass (this task touches no code any test exercises, so this is a pure regression check).

- [ ] **Step 6: Manual verification**

Run the built app (`./build/PsyClientManager` or this platform's equivalent build output path), then:
1. Open Settings → General → Backup.
2. Click "Create backup...", choose a destination, confirm a "Backup Created" dialog appears and the `.psybackup` file exists at that path.
3. Click "Validate backup...", select the file just created, confirm a "Backup Valid" dialog appears.
4. Click "Validate backup..." again, select a non-`.psybackup` file (or a `.psybackup` file with a byte manually corrupted, e.g. via a hex editor or `dd`), confirm a "Backup Invalid" dialog appears listing at least one error.

- [ ] **Step 7: Commit**

```bash
git add src/app/settings_dialog.cpp
git commit -m "feat(app): add Validate backup action to Settings"
```

---

### Task 5: Docs, version, and changelog

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/app/application.cpp`
- Modify: `CHANGELOG.md`
- Modify: `README.md`

**Interfaces:**
- Consumes: nothing.
- Produces: nothing consumed by a later task — final task.

- [ ] **Step 1: Bump the project version**

In `CMakeLists.txt`, replace:

```cmake
project(PsyClientManager VERSION 0.1.9 LANGUAGES CXX)
```

with:

```cmake
project(PsyClientManager VERSION 0.1.10 LANGUAGES CXX)
```

(If the version in this file has moved past `0.1.9` by the time this task runs — check with `grep -n "project(PsyClientManager" CMakeLists.txt` first — bump from whatever the current value is by one patch version instead, and use that same new value in Steps 2 and 3 below.)

- [ ] **Step 2: Bump the application version string**

In `src/app/application.cpp`, replace:

```cpp
  app.setApplicationVersion("0.1.9");
```

with:

```cpp
  app.setApplicationVersion("0.1.10");
```

- [ ] **Step 3: Add a CHANGELOG entry**

In `CHANGELOG.md`, insert this new section right after the `# Changelog` header and its intro line, before the existing most-recent `## [0.1.9]` entry:

```markdown
## [0.1.10] - 2026-07-26

### Added

- "Create backup..." and "Validate backup..." actions in Settings, backed
  by the local `.psybackup` backup service.

```

- [ ] **Step 4: Update the README feature list**

In `README.md`, replace:

```markdown
- Local `.psybackup` backups of the database (and optionally attachments), with checksum validation
```

with:

```markdown
- Local `.psybackup` backups of the database and attachments, created and validated from Settings
```

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/app/application.cpp CHANGELOG.md README.md
git commit -m "docs: document backup UI and bump version"
```
