# Clean Up Left-Nav Information Architecture Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Remove the `Details`/`Notes` buttons from the primary left navigation and reach the client card and notes feed only by selecting a client, per issue #49 and `docs/superpowers/specs/2026-09-23-clients-nav-redesign-design.md`.

**Architecture:** A new `ClientWorkspacePage` widget owns the existing `ClientInfo` list, `QClientInfoCardPage`, and `ClientNotesPage` unchanged, and switches between a list page and a detail page (Info/Notes tabs + back button) inside its own `QStackedWidget`. `MainWindow` hosts one `ClientWorkspacePage` instead of three separate top-level pages, and its `Pages` enum shrinks to `clientInfo`, `eventInfo`, `analytics`.

**Tech Stack:** Qt6 Widgets, CMake, GoogleTest (existing suites only — no new widget-level tests; see Global Constraints).

## Global Constraints

- No test file in this repo constructs a `QApplication`; do not introduce one for this task — `ClientWorkspacePage` is verified by manual pass through `scripts/run-dev-isolated.sh`, never launch the app any other way (it touches the real `~/.config/PsyClientManager`).
- Neither `ClientInfo`'s row delegate (`src/client_model/qclient_delegate.cpp`) nor the internal layout of `QClientInfoCardPage`/`ClientNotesPage` changes — they keep the full page width they already assume.
- `Settings`/`About` utility buttons are unchanged.
- Every new user-facing string needs a `tr()` call and must be filled into both `translation/app_ru.ts` and `translation/app_en.ts` (no `type="unfinished"` entries) before the final commit, per this project's CI gate.
- Rebuild with `cmake --build .` and run `ctest --output-on-failure` from `build/` after every task that touches `main_window.cpp`/`.h` — confirm the exit summary line, don't trust a truncated log tail.

---

### Task 1: `ClientWorkspacePage` skeleton (list/detail stack, no wiring)

**Files:**
- Create: `src/pages/client_info_page/client_workspace_page.h`
- Create: `src/pages/client_info_page/client_workspace_page.cpp`
- Modify: `src/pages/client_info_page/CMakeLists.txt`

**Interfaces:**
- Consumes: `ClientInfo(std::shared_ptr<QClientModel>, QWidget*)` (`src/pages/client_info_page/client_info.h`), `QClientInfoCardPage(std::shared_ptr<pcm::database::Database>, QWidget*)` (`src/pages/detail_client_info_page/client_info_card.h`), `ClientNotesPage(std::shared_ptr<pcm::database::Database>, QWidget*)` (`src/pages/client_notes_page/client_notes_page.h`).
- Produces: `class ClientWorkspacePage : public QWidget` with public accessors `ClientInfo *clientList() const`, `QClientInfoCardPage *clientCard() const`, `ClientNotesPage *clientNotes() const`, and a constructor `ClientWorkspacePage(std::shared_ptr<QClientModel> clientModel, std::shared_ptr<pcm::database::Database> db, QWidget *parent = nullptr)`. Used by Task 2 (adds `selectClient`) and Task 3 (`MainWindow`).

- [ ] **Step 1: Write the header**

```cpp
// src/pages/client_info_page/client_workspace_page.h
#pragma once

#include "client_info.h"
#include "client_info_card.h"
#include "client_notes_page.h"

#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QWidget>

#include <memory>

class ClientWorkspacePage final : public QWidget {
  Q_OBJECT

public:
  explicit ClientWorkspacePage(std::shared_ptr<QClientModel> clientModel,
                               std::shared_ptr<pcm::database::Database> db,
                               QWidget *parent = nullptr);

  [[nodiscard]] ClientInfo *clientList() const { return mClientList; }
  [[nodiscard]] QClientInfoCardPage *clientCard() const { return mClientCard; }
  [[nodiscard]] ClientNotesPage *clientNotes() const { return mClientNotes; }

signals:
  void removeButtonClicked(int64_t clientId);
  void provideSaveClient(const DuckClient &client);
  void openEventRequested(int64_t eventId, qint64 dayMs);

public slots:
  void selectClient(const std::optional<DuckClient> &client, int tabIndex);
  void showList();

private:
  QStackedWidget *mStack{nullptr};
  QTabWidget *mDetailTabs{nullptr};
  QPushButton *mBackButton{nullptr};
  ClientInfo *mClientList{nullptr};
  QClientInfoCardPage *mClientCard{nullptr};
  ClientNotesPage *mClientNotes{nullptr};

  enum StackIndex { kListPage = 0, kDetailPage = 1 };
};
```

- [ ] **Step 2: Write the implementation**

```cpp
// src/pages/client_info_page/client_workspace_page.cpp
#include "client_workspace_page.h"

#include <QVBoxLayout>

ClientWorkspacePage::ClientWorkspacePage(
    std::shared_ptr<QClientModel> clientModel,
    std::shared_ptr<pcm::database::Database> db, QWidget *parent)
    : QWidget(parent) {
  mClientList = new ClientInfo(std::move(clientModel), this);
  mClientCard = new QClientInfoCardPage(db, this);
  mClientNotes = new ClientNotesPage(std::move(db), this);

  mBackButton = new QPushButton(tr("Back to clients"), this);
  mBackButton->setFlat(true);
  mBackButton->setCursor(Qt::PointingHandCursor);

  mDetailTabs = new QTabWidget(this);
  mDetailTabs->addTab(mClientCard, tr("Info"));
  mDetailTabs->addTab(mClientNotes, tr("Notes"));

  auto *detailPage = new QWidget(this);
  auto *detailLayout = new QVBoxLayout(detailPage);
  detailLayout->setContentsMargins(0, 0, 0, 0);
  detailLayout->setSpacing(4);
  detailLayout->addWidget(mBackButton, 0, Qt::AlignLeft);
  detailLayout->addWidget(mDetailTabs, 1);

  mStack = new QStackedWidget(this);
  mStack->insertWidget(kListPage, mClientList);
  mStack->insertWidget(kDetailPage, detailPage);
  mStack->setCurrentIndex(kListPage);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(mStack);

  connect(mBackButton, &QPushButton::clicked, this,
          &ClientWorkspacePage::showList);
}

void ClientWorkspacePage::selectClient(
    const std::optional<DuckClient> &client, const int tabIndex) {
  mClientCard->setClientInfo(client);
  mClientNotes->setClientInfo(client);
  mDetailTabs->setCurrentIndex(tabIndex);
  mStack->setCurrentIndex(kDetailPage);
}

void ClientWorkspacePage::showList() {
  mStack->setCurrentIndex(kListPage);
}
```

- [ ] **Step 3: Wire the new files into the build**

Edit `src/pages/client_info_page/CMakeLists.txt`:

```cmake
qt_add_library(${TARGET_NAME} STATIC
        client_info.cpp
        client_workspace_page.cpp
)
```

and extend both `add_dependencies` and `target_link_libraries` with
`${PROJECT_NAME}_client_info_card_page` and `${PROJECT_NAME}_client_notes_page`
(both already depend on `config`/`database`/`event_view`/`widgets`, and
neither depends back on `${PROJECT_NAME}_client_page`, so this does not
introduce a cycle — confirmed by `grep -rn client_page src/pages/detail_client_info_page src/pages/client_notes_page`, which returns nothing):

```cmake
add_dependencies(${TARGET_NAME}
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_client_model
        ${PROJECT_NAME}_client_info_card_page
        ${PROJECT_NAME}_client_notes_page
)

target_link_libraries(${TARGET_NAME} PUBLIC
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_client_model
        ${PROJECT_NAME}_client_info_card_page
        ${PROJECT_NAME}_client_notes_page
        Qt6::Widgets
)
```

- [ ] **Step 4: Build to verify it compiles**

Run (from `build/`, reusing the existing configured cache — do not create a
fresh build dir, vcpkg's registry fetch can hang for a new one):

```bash
cmake --build . -j$(nproc)
```

Expected: the whole project still links; `ClientWorkspacePage` is compiled
into `PsyClientManager_client_page` but nothing references it yet, so no
other target changes.

- [ ] **Step 5: Commit**

```bash
git add src/pages/client_info_page/client_workspace_page.h \
        src/pages/client_info_page/client_workspace_page.cpp \
        src/pages/client_info_page/CMakeLists.txt
git commit -m "feat: add ClientWorkspacePage skeleton (list/detail stack)"
```

---

### Task 2: Wire client selection and cross-navigation inside `ClientWorkspacePage`

**Files:**
- Modify: `src/pages/client_info_page/client_workspace_page.cpp`

**Interfaces:**
- Consumes: `ClientInfo::displayButtonClicked(const std::optional<DuckClient>&)`, `ClientInfo::notesButtonClicked(const std::optional<DuckClient>&)`, `ClientInfo::removeButtonClicked(int64_t)` (`client_info.h`); `ClientNotesPage::openClientCardRequested(const std::optional<DuckClient>&)`, `ClientNotesPage::openEventRequested(int64_t, qint64)` (`client_notes_page.h`); `QClientInfoCardPage::provideSaveClient(const DuckClient&)` (`client_info_card.h`).
- Produces: no new public interface — completes the signal graph declared in Task 1's header (`removeButtonClicked`, `provideSaveClient`, `openEventRequested` now actually fire).

- [ ] **Step 1: Add the wiring at the end of the constructor**

Append to `ClientWorkspacePage::ClientWorkspacePage` in
`src/pages/client_info_page/client_workspace_page.cpp`, right after the
existing `connect(mBackButton, ...)` line:

```cpp
  connect(mClientList, &ClientInfo::displayButtonClicked, this,
          [this](const std::optional<DuckClient> &client) {
            selectClient(client, 0);
          });
  connect(mClientList, &ClientInfo::notesButtonClicked, this,
          [this](const std::optional<DuckClient> &client) {
            selectClient(client, 1);
          });
  connect(mClientList, &ClientInfo::removeButtonClicked, this,
          &ClientWorkspacePage::removeButtonClicked);

  connect(mClientNotes, &ClientNotesPage::openClientCardRequested, this,
          [this](const std::optional<DuckClient> &client) {
            selectClient(client, 0);
          });
  connect(mClientNotes, &ClientNotesPage::openEventRequested, this,
          &ClientWorkspacePage::openEventRequested);

  connect(mClientCard, &QClientInfoCardPage::provideSaveClient, this,
          &ClientWorkspacePage::provideSaveClient);
```

- [ ] **Step 2: Build to verify it compiles**

```bash
cmake --build . -j$(nproc)
```

Expected: clean build, same as Task 1 — this widget is still unused by
`MainWindow`, so behavior is only verifiable by reading the code at this
point; Task 4 covers the manual run.

- [ ] **Step 3: Commit**

```bash
git add src/pages/client_info_page/client_workspace_page.cpp
git commit -m "feat: wire client selection and cross-navigation in ClientWorkspacePage"
```

---

### Task 3: Replace the three top-level client pages with `ClientWorkspacePage` in `MainWindow`

**Files:**
- Modify: `src/app/main_window.h`
- Modify: `src/app/main_window.cpp`
- Modify: `src/app/application.cpp`

**Interfaces:**
- Consumes: `ClientWorkspacePage` from Task 1/2 (`src/pages/client_info_page/client_workspace_page.h`).
- Produces: `MainWindow::addClientInfoPage(std::shared_ptr<QClientModel>, std::shared_ptr<pcm::database::Database>)` (signature change — was `(std::shared_ptr<QClientModel>)`); `MainWindow::addClientCardPage`/`addClientNotesPage` are removed. `MainWindow::Pages` now has exactly `clientInfo`, `eventInfo`, `analytics`.

- [ ] **Step 1: Update `src/app/main_window.h`**

Change the `Pages` enum:

```cpp
enum class Pages { clientInfo, eventInfo, analytics };
```

Change the `addClientInfoPage` declaration and remove the other two:

```cpp
void addClientInfoPage(std::shared_ptr<QClientModel> model,
                       std::shared_ptr<pcm::database::Database> db);
void addEventInfoPage(QTimelineModel *model);
void addAnalyticsPage(std::shared_ptr<pcm::database::Database> db);
```

(delete the `addClientCardPage`/`addClientNotesPage` lines entirely).

Remove `mBtnProfile` and `mBtnNotes` member declarations, and remove the
`setClientNavigationVisible` declaration. Add the `#include
"client_workspace_page.h"` near the top (replacing the now-unused
`client_info.h`/`client_notes_page.h`/`client_info_card.h` includes if
nothing else in this header needs them — check with `grep -n
"ClientInfo\|ClientNotesPage\|QClientInfoCardPage" src/app/main_window.h`
after this edit and drop any include whose type no longer appears).

- [ ] **Step 2: Update the constructor in `src/app/main_window.cpp`**

Remove these two lines (and their `hide()` calls) from the constructor:

```cpp
  mBtnProfile =
      new TabButton(QIcon(":/icons/users-gear-solid-full.svg"), tr(": NAV_DETAILS"), this);
  mBtnNotes =
      new TabButton(QIcon(":/icons/notes.svg"), tr("Notes"), this);
```
```cpp
  mUi->verticalLayout->addWidget(mBtnProfile);
  mUi->verticalLayout->addWidget(mBtnNotes);
  mBtnProfile->hide();
  mBtnNotes->hide();
```

(keep the three `mBtnCalendar`/`mBtnClients`/`mBtnAnalytics` construction
and `addWidget` lines unchanged).

- [ ] **Step 3: Replace `addClientInfoPage`/`addClientCardPage`/`addClientNotesPage`**

Replace the three method bodies with one:

```cpp
void MainWindow::addClientInfoPage(std::shared_ptr<QClientModel> model,
                                   std::shared_ptr<pcm::database::Database> db) {
  const auto page = new ClientWorkspacePage(std::move(model), std::move(db), this);
  mPages.insertOrAssign(Pages::clientInfo, page);

  const int index = mUi->stackedWidget->addWidget(page);
  mPagesIndex.insertOrAssign(Pages::clientInfo, index);

  mClientPageActions = new QWidget(this);
  auto *actionsLayout = new QHBoxLayout(mClientPageActions);
  actionsLayout->setContentsMargins(0, 0, 0, 0);
  actionsLayout->setSpacing(pcm::widgets::constants::kPanelPadding);

  mClientSearchInput = new oclero::qlementine::LineEdit(mClientPageActions);
  mClientSearchInput->setPlaceholderText(tr("Search clients"));
  mClientSearchInput->setClearButtonEnabled(true);
  mClientSearchInput->setIcon(QIcon(":/icons/user-solid-full.svg"));
  mClientSearchInput->setMinimumWidth(260);

  mShowInactiveClientsSwitch =
      new oclero::qlementine::Switch(mClientPageActions);
  mShowInactiveClientsSwitch->setText(tr("Show inactive"));

  mAddClientButton = new QPushButton(QIcon(":/icons/user-plus-solid-full.svg"),
                                     tr("Add client"), mClientPageActions);
  mAddClientButton->setIconSize(QSize(16, 16));
  mAddClientButton->setCursor(Qt::PointingHandCursor);

  actionsLayout->addWidget(mClientSearchInput, 1);
  actionsLayout->addWidget(mShowInactiveClientsSwitch, 0);
  actionsLayout->addWidget(mAddClientButton, 0);
  setPageCustomWidget(Pages::clientInfo, mClientPageActions);
}
```

This keeps the header bar (search/show-inactive/add-client) exactly as it
was — only the page content changed from a bare `ClientInfo` to a
`ClientWorkspacePage`. Delete the old `addClientCardPage` and
`addClientNotesPage` method bodies entirely.

- [ ] **Step 4: Rewrite `connectSignals()`**

Replace the body of `MainWindow::connectSignals()` with:

```cpp
void MainWindow::connectSignals() {
  const auto clientWorkspace =
      dynamic_cast<ClientWorkspacePage *>(mPages[Pages::clientInfo]);
  const auto eventInfoPage =
      dynamic_cast<QEventInfoPage *>(mPages[Pages::eventInfo]);

  connect(mBtnCalendar, &QPushButton::clicked,
          [this]() { showPage(Pages::eventInfo, mBtnCalendar); });

  connect(mBtnClients, &QPushButton::clicked,
          [this]() { showPage(Pages::clientInfo, mBtnClients); });

  connect(mBtnAnalytics, &QPushButton::clicked,
          [this]() { showPage(Pages::analytics, mBtnAnalytics); });

  connect(clientWorkspace, &ClientWorkspacePage::openEventRequested,
          [this, eventInfoPage](const int64_t eventId, const qint64 dayMs) {
            eventInfoPage->openEventOnDay(eventId, dayMs);
            showPage(Pages::eventInfo, mBtnCalendar);
          });

  connect(clientWorkspace, &ClientWorkspacePage::removeButtonClicked, this,
          [this](const int64_t clientId) { emit provideRemoveClient(clientId); });

  connect(mClientSearchInput, &QLineEdit::textChanged,
          clientWorkspace->clientList(), &ClientInfo::setSearchQuery);
  connect(mShowInactiveClientsSwitch, &QAbstractButton::toggled,
          clientWorkspace->clientList(), &ClientInfo::setShowInactiveClients);

  connect(mAddClientButton, &QPushButton::clicked, this, [this, clientWorkspace]() {
    clientWorkspace->selectClient(std::nullopt, 0);
    clientWorkspace->clientCard()->enterInEditMode();
  });

  connect(clientWorkspace, &ClientWorkspacePage::provideSaveClient,
          [&](const auto &client) { emit provideSaveClient(client); });

  connect(eventInfoPage, &QEventInfoPage::provideClientEventPairSave,
          [this](const int64_t clientId, const int64_t eventId) {
            emit provideClientEventPairSave(clientId, eventId);
          });

  showPage(Pages::eventInfo, mBtnCalendar);
}
```

Note what changed: `clientCardPage`/`clientNotesPage`/`clientInfoPage`
locals are gone (only `clientWorkspace` and `eventInfoPage` remain), the
two `mBtnProfile`/`mBtnNotes` `connect` calls are gone, and every place
that previously called `setClientNavigationVisible(...)` +
`showPage(Pages::clientCard/clientNotes, ...)` is gone — that job is now
`ClientWorkspacePage::selectClient`'s, already wired internally in Task 2.

- [ ] **Step 5: Update `checkButton`, `setClientNavigationVisible`, `pageTitle`, `refreshPageAppearance`**

In `checkButton` (`src/app/main_window.cpp`), delete the two lines
resetting `mBtnProfile`/`mBtnNotes`:

```cpp
void MainWindow::checkButton(QPushButton *btn) const {
  mBtnCalendar->setChecked(false);
  mBtnClients->setChecked(false);
  mBtnAnalytics->setChecked(false);
  btn->setChecked(true);
}
```

Delete `MainWindow::setClientNavigationVisible` entirely (its declaration
was already removed from the header in Step 1).

In `pageTitle`, drop the two removed cases:

```cpp
QString MainWindow::pageTitle(const Pages page) const {
  switch (page) {
    case Pages::clientInfo:
      return tr("Clients");
    case Pages::eventInfo:
      return tr("Calendar");
    case Pages::analytics:
      return tr("Analytics");
  }

  return tr("Page");
}
```

In `refreshPageAppearance`, replace the `Pages::clientNotes` lookup (that
enum value no longer exists) with a lookup through the workspace:

```cpp
  if (const auto workspace =
          dynamic_cast<ClientWorkspacePage *>(mPages.value(Pages::clientInfo, nullptr))) {
    workspace->clientNotes()->refresh();
  }
```

- [ ] **Step 6: Update the call site in `src/app/application.cpp`**

Change:

```cpp
  mMainWindow->addEventInfoPage(new QTimelineModel(mDb, this));
  mMainWindow->addClientInfoPage(mClientModel);
  mMainWindow->addAnalyticsPage(mDb);
  mMainWindow->addClientCardPage(mDb);
  mMainWindow->addClientNotesPage(mDb);
```

to:

```cpp
  mMainWindow->addEventInfoPage(new QTimelineModel(mDb, this));
  mMainWindow->addClientInfoPage(mClientModel, mDb);
  mMainWindow->addAnalyticsPage(mDb);
```

- [ ] **Step 7: Build and run the full test suite**

```bash
cmake --build . -j$(nproc)
ctest --output-on-failure
```

Expected: a clean build (grep the log for `error` before trusting a
truncated tail) and the same pass count as before this task — this
refactor touches no logic covered by the existing suites, so 0 new
failures is the bar, not new passing tests.

- [ ] **Step 8: Commit**

```bash
git add src/app/main_window.h src/app/main_window.cpp src/app/application.cpp
git commit -m "refactor: host client card/notes inside ClientWorkspacePage, drop Details/Notes nav buttons"
```

---

### Task 4: Translations, manual verification, version bump, and PR

**Files:**
- Modify: `translation/app_ru.ts`, `translation/app_en.ts`
- Modify: `CMakeLists.txt`, `src/app/application.cpp` (version bump)
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Regenerate translations**

```bash
cmake --build . --target update_translations
```

Expected: `lupdate` reports the new source strings from Task 1 ("Back to
clients", "Info", "Notes" tab labels — note "Notes" as a tab label is a
*new* source string distinct from the old top-level nav button text that
`lupdate -no-obsolete` already dropped, so it needs its own translation
entry) as `0 new` only after Step 2 below fills them in — run this first
to see the `type="unfinished"` entries appear.

- [ ] **Step 2: Fill in the new translation entries**

Open `translation/app_ru.ts` and `translation/app_en.ts`, find every
`<translation type="unfinished"></translation>` introduced by Step 1
(there should be exactly the new strings from Task 1: "Back to clients",
"Info", and the tab-label "Notes" if it wasn't already an existing
`<source>` — check with
`grep -n 'type="unfinished"' translation/app_ru.ts translation/app_en.ts`
first). Fill the Russian translation into `app_ru.ts` and the identical
source text into `app_en.ts` (English is the source language), removing
`type="unfinished"` from each, matching the pattern already used
throughout both files.

- [ ] **Step 3: Confirm zero unfinished entries and rebuild**

```bash
grep -n 'type="unfinished"' translation/app_ru.ts translation/app_en.ts
cmake --build . --target update_translations
```

Expected: the `grep` finds nothing (exit code 1); the second command
reports `0 new` for both files, confirming the fill-in was complete and
the diff is now stable.

- [ ] **Step 4: Full rebuild and test run**

```bash
cmake --build . -j$(nproc)
ctest --output-on-failure
```

Expected: clean build, same pass count as Task 3.

- [ ] **Step 5: Manual GUI verification**

```bash
scripts/run-dev-isolated.sh ./build/PsyClientManager
```

Never launch the binary any other way — this script keeps the run off the
real `~/.config/PsyClientManager`. Walk through:
1. Left nav shows exactly Calendar, Clients, Analytics as primary items
   (no Details/Notes button ever appears), with Settings/About still
   pinned and muted at the bottom.
2. On Clients, click a client's "Display" action → detail page opens on
   the Info tab with that client's data.
3. Click "Back to clients" → returns to the full list.
4. Click a client's "Notes" action → detail page opens on the Notes tab
   directly.
5. From Notes, use "open client card" → switches to the Info tab, same
   client, without reloading.
6. From Notes, open an event → switches to the Calendar page on that
   event's day.
7. "Add client" → detail page opens on the Info tab in edit mode with
   empty fields.

If any step fails, stop and re-open Task 3 rather than patching around it
here — this step is verification, not a place to redesign the wiring.

- [ ] **Step 6: Version bump and changelog**

Bump `project(PsyClientManager VERSION ...)` in `CMakeLists.txt` and
`app.setApplicationVersion("...")` in `src/app/application.cpp` to the
next patch version after whatever `main` is on at merge time (check
`grep 'project(PsyClientManager VERSION' CMakeLists.txt` on latest `main`
first — do not assume the version this plan was written against is still
current). Add a `CHANGELOG.md` entry under that version, `### Changed`,
one bullet describing the nav simplification (Details/Notes reached only
via a selected client; primary nav is Calendar/Clients/Analytics).

- [ ] **Step 7: Commit, push, and open the PR**

```bash
git add translation/app_ru.ts translation/app_en.ts CMakeLists.txt \
        src/app/application.cpp CHANGELOG.md
git commit -m "chore: translations and version bump for #49 nav redesign"
git push -u origin <branch-name>
gh pr create --title "Clean up left-nav information architecture" \
  --body "Closes #49. See docs/superpowers/specs/2026-09-23-clients-nav-redesign-design.md for the design."
```

Wait for CI (`gh pr checks <number>`) before reporting the PR as ready.
