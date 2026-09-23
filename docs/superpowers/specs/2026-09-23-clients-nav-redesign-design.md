# Clean Up Left-Nav Information Architecture Design

## Scope

Issue #49 removes the ambiguity between the primary left-nav destinations
(Calendar, Clients, Analytics) and the client-scoped "Details" and "Notes"
pages, which today appear and disappear in the same button list once a
client is selected — visually indistinguishable from a permanent top-level
destination. Settings and About are already secondary, muted utility
buttons pinned below the primary list and are unaffected. The internal
layout of the client card fields and the Notes feed (#48) is unaffected;
this issue is only about where they are reached from.

## Current State

`MainWindow::Pages` has five top-level entries: `clientInfo`, `eventInfo`,
`analytics`, `clientCard`, `clientNotes`, each a full page in
`mUi->stackedWidget`. `mBtnProfile` ("Details") and `mBtnNotes` ("Notes")
are hidden at startup and shown via `setClientNavigationVisible` only once
a client is selected, but they render in the same vertical button list as
`mBtnCalendar`/`mBtnClients`/`mBtnAnalytics`, so a user has no visual cue
that they are contextual rather than permanent. Selecting a client on the
`ClientInfo` list (`displayButtonClicked`/`notesButtonClicked`) currently
replaces the *entire* window content with the client card or notes page.

## Proposed Architecture

`mBtnProfile` and `mBtnNotes` are removed entirely; the primary nav list
permanently shows only Calendar, Clients, and Analytics (Settings/About
keep their existing muted, pinned-to-bottom placement — they already read
as a distinct, secondary group and need no change).

A new composite widget, `ClientWorkspacePage`, replaces the direct use of
`ClientInfo` on `Pages::clientInfo`. It owns the existing `ClientInfo` list
unchanged, plus a detail panel that appears once a client is selected,
containing a `QTabWidget` with two tabs — "Info" (hosts the existing
`QClientInfoCardPage`) and "Notes" (hosts the existing `ClientNotesPage`).
Neither child widget's internals change; `ClientWorkspacePage` only
reparents them and forwards `setClientInfo` calls:

```cpp
class ClientWorkspacePage final : public QWidget {
  Q_OBJECT
public:
  explicit ClientWorkspacePage(std::shared_ptr<QClientModel> clientModel,
                               std::shared_ptr<pcm::database::Database> db,
                               QWidget *parent = nullptr);

  ClientInfo *clientList() const;
  QClientInfoCardPage *clientCard() const;
  ClientNotesPage *clientNotes() const;

signals:
  void removeButtonClicked(int64_t clientId);
  void provideSaveClient(const DuckClient &client);
  void openEventRequested(int64_t eventId, qint64 dayMs);

public slots:
  void selectClient(const std::optional<DuckClient> &client, int tabIndex);

private:
  QStackedWidget *mStack{nullptr};
  QTabWidget *mDetailTabs{nullptr};
  ClientInfo *mClientList{nullptr};
  QClientInfoCardPage *mClientCard{nullptr};
  ClientNotesPage *mClientNotes{nullptr};
};
```

`selectClient(client, tabIndex)` calls `setClientInfo` on both children,
sets `mDetailTabs->setCurrentIndex(tabIndex)`, and switches `mStack` to the
detail page; it is the single entry point used by every existing trigger
(`displayButtonClicked` → tab 0, `notesButtonClicked` → tab 1,
`ClientNotesPage::openClientCardRequested` → tab 0, "Add client" → tab 0
in edit mode). The back button switches `mStack` back to the list page
without clearing the loaded client, so reopening the same client's detail
(e.g. via "Add client" → cancel → reselect) does not need a reload.

`MainWindow::Pages` shrinks to `clientInfo`, `eventInfo`, `analytics`.
`setClientNavigationVisible` and the `mPagesIndex`/`checkButton` branches
for `clientCard`/`clientNotes` are deleted; `MainWindow::connectSignals`
wires the surviving `ClientWorkspacePage` signals instead of reaching into
`QClientInfoCardPage`/`ClientNotesPage` directly. `getPage(Pages::clientCard)`
and `getPage(Pages::clientNotes)` have no remaining callers (verified by
grep before removal) and are dropped from the enum.

## Layout

`QClientDelegate::paint` renders each row of the client list as a full-width
card (name, contacts, last session, status, inline action icons), sized to
`option.rect.width()` — it is not designed to fit a narrow master column.
Rather than shrink it (out of scope: it would need real visual redesign
work of its own), `ClientWorkspacePage` uses an internal `QStackedWidget`
with two full-width pages: the existing `ClientInfo` list, and a detail
page holding the `QTabWidget` (Info/Notes) plus a "Back to clients" button
above it. Selecting a client switches the internal stack to the detail
page; the back button (and, once #48/#49 land, any future breadcrumb)
switches back to the list. Neither `ClientInfo`'s row delegate nor
`QClientInfoCardPage`/`ClientNotesPage`'s internal layout needs to change —
each keeps the full page width it already assumes.

## Out of Scope

- Any change to the fields on the client card or the Notes feed (#48).
- Making Settings a full page instead of a modal dialog — it already reads
  as a distinct entity and satisfies the acceptance criteria as-is.
- Cross-page entry points from event cards into the client card: grepped
  for existing usage and found none beyond the `ClientInfo` list itself.

## Failure Handling and Tests

`selectClient(std::nullopt, ...)` (e.g. after the selected client is
removed) hides the detail panel and shows the placeholder rather than
leaving stale data on the tabs. GoogleTest coverage adds
`ClientWorkspacePageTest` cases: selecting a client shows the detail panel
on the requested tab, `notesButtonClicked` opens the Notes tab directly,
`openClientCardRequested` from Notes switches to the Info tab without
losing the currently loaded client, and clearing the selection restores the
placeholder. Existing `ClientInfo`, `QClientInfoCardPage`, and
`ClientNotesPage` test suites are unaffected since their internals are
untouched.
