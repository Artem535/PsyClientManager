# Notes screen → chat-like client journal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Evolve `ClientNotesPage` into a date-grouped, chat-like client journal: breadcrumb header with a client-card link, date dividers, compact attachments that expand on click, a faster composer (Ctrl+Enter + save confirmation), and an attachments-only feed filter.

**Architecture:** Pure UI/presentation change to `src/pages/client_notes_page/client_notes_page.{h,cpp}` plus one new cross-page signal wired in `main_window.cpp`. No schema changes, no new `Database` methods.

**Tech Stack:** Qt6 (QWidget, QLabel, QPushButton, QComboBox, QTimer), existing `pcm::database::Database` read/write methods, existing `pcm::widgets::constants`.

## Global Constraints

- No DB schema changes, no new `Database` methods (design doc: `docs/superpowers/specs/2026-07-30-notes-journal-design.md`).
- Session-linking, system-event lines, last/next-appointment header, and in-place editing are explicitly out of scope (deferred to #26/#28/#30).
- No dedicated unit test for `ClientNotesPage` — it has zero existing test coverage and is not linked into any test target (same precedent as `meeting_utils.cpp`/`confirmation_utils.cpp`). Each task verifies via a clean build plus the existing `ctest` suite as a regression check.
- Every task ends with `cmake --build build-release --target PsyClientManager --parallel` succeeding and `ctest --test-dir build-release --output-on-failure` passing 100%.
- Do not develop directly on `main` — this plan executes on `feat/48-notes-journal` (already created).

---

### Task 1: Breadcrumb header and client-card navigation signal

**Files:**
- Modify: `src/pages/client_notes_page/client_notes_page.h`
- Modify: `src/pages/client_notes_page/client_notes_page.cpp`
- Modify: `src/app/main_window.cpp`

**Interfaces:**
- Produces: `void ClientNotesPage::openClientCardRequested(const std::optional<DuckClient> &client);` — a new signal, connected in `main_window.cpp` to navigate to the client card page.

- [ ] **Step 1: Add the signal, slot, and button member; remove the unused title label**

In `src/pages/client_notes_page/client_notes_page.h`, replace:

```cpp
public slots:
  void setClientInfo(const std::optional<DuckClient> &client);
  void refresh();

private slots:
  void onAddNoteClicked();
  void onAttachFilesClicked();
  void onPendingAttachmentActivated(QListWidgetItem *item);
```

with:

```cpp
signals:
  void openClientCardRequested(const std::optional<DuckClient> &client);

public slots:
  void setClientInfo(const std::optional<DuckClient> &client);
  void refresh();

private slots:
  void onAddNoteClicked();
  void onAttachFilesClicked();
  void onPendingAttachmentActivated(QListWidgetItem *item);
  void onOpenClientCardClicked();
```

Then replace the member block:

```cpp
  QLabel *mTitleLabel = nullptr;
  QLabel *mClientNameLabel = nullptr;
```

with:

```cpp
  QLabel *mClientNameLabel = nullptr;
  QPushButton *mOpenClientCardButton = nullptr;
```

- [ ] **Step 2: Rebuild the header in `buildUi`**

In `src/pages/client_notes_page/client_notes_page.cpp`, replace the header block:

```cpp
  auto *headerSurface = makeSurface(this);
  auto *headerLayout = new QVBoxLayout(headerSurface);
  headerLayout->setContentsMargins(
      pcm::widgets::constants::kNotesHeaderHorizontalPadding,
      pcm::widgets::constants::kNotesHeaderVerticalPadding,
      pcm::widgets::constants::kNotesHeaderHorizontalPadding,
      pcm::widgets::constants::kNotesHeaderVerticalPadding);
  headerLayout->setSpacing(4);

  mTitleLabel = new QLabel(tr("Notes"), headerSurface);
  auto titleFont = mTitleLabel->font();
  titleFont.setPointSize(titleFont.pointSize() + 3);
  titleFont.setBold(true);
  mTitleLabel->setFont(titleFont);
  mTitleLabel->setStyleSheet("color: rgba(255, 255, 255, 0.92);");

  mClientNameLabel = new QLabel(tr("No client selected"), headerSurface);
  mClientNameLabel->setStyleSheet("color: rgba(255, 255, 255, 0.60);");

  headerLayout->addWidget(mTitleLabel);
  headerLayout->addWidget(mClientNameLabel);
  rootLayout->addWidget(headerSurface);
```

with:

```cpp
  auto *headerSurface = makeSurface(this);
  auto *headerLayout = new QHBoxLayout(headerSurface);
  headerLayout->setContentsMargins(
      pcm::widgets::constants::kNotesHeaderHorizontalPadding,
      pcm::widgets::constants::kNotesHeaderVerticalPadding,
      pcm::widgets::constants::kNotesHeaderHorizontalPadding,
      pcm::widgets::constants::kNotesHeaderVerticalPadding);
  headerLayout->setSpacing(10);

  mClientNameLabel = new QLabel(tr("No client selected"), headerSurface);
  auto clientNameFont = mClientNameLabel->font();
  clientNameFont.setPointSize(clientNameFont.pointSize() + 2);
  clientNameFont.setBold(true);
  mClientNameLabel->setFont(clientNameFont);
  mClientNameLabel->setStyleSheet("color: rgba(255, 255, 255, 0.92);");

  mOpenClientCardButton = new QPushButton(tr("Open client card"), headerSurface);
  mOpenClientCardButton->setCursor(Qt::PointingHandCursor);
  mOpenClientCardButton->setFlat(true);
  mOpenClientCardButton->setEnabled(false);

  headerLayout->addWidget(mClientNameLabel);
  headerLayout->addStretch();
  headerLayout->addWidget(mOpenClientCardButton);
  rootLayout->addWidget(headerSurface);
```

At the end of `buildUi`, after the existing three `connect(...)` calls, add a fourth:

```cpp
  connect(mAttachFilesButton, &QPushButton::clicked, this,
          &ClientNotesPage::onAttachFilesClicked);
  connect(mAddNoteButton, &QPushButton::clicked, this,
          &ClientNotesPage::onAddNoteClicked);
  connect(mPendingAttachmentsList, &QListWidget::itemDoubleClicked, this,
          &ClientNotesPage::onPendingAttachmentActivated);
  connect(mOpenClientCardButton, &QPushButton::clicked, this,
          &ClientNotesPage::onOpenClientCardClicked);
```

- [ ] **Step 3: Update `setClientInfo` to build the breadcrumb text and enable the button**

Replace:

```cpp
void ClientNotesPage::setClientInfo(const std::optional<DuckClient> &client) {
  mCurrentClient = client;
  mPendingAttachments.clear();
  refreshPendingAttachments();
  mClientNameLabel->setText(currentClientTitle());
  reloadNotes();
}
```

with:

```cpp
void ClientNotesPage::setClientInfo(const std::optional<DuckClient> &client) {
  mCurrentClient = client;
  mPendingAttachments.clear();
  refreshPendingAttachments();
  mClientNameLabel->setText(client.has_value()
                                ? tr("%1 → Notes").arg(currentClientTitle())
                                : currentClientTitle());
  mOpenClientCardButton->setEnabled(client.has_value());
  reloadNotes();
}
```

- [ ] **Step 4: Implement `onOpenClientCardClicked`**

Add after `onPendingAttachmentActivated`'s closing brace:

```cpp
void ClientNotesPage::onOpenClientCardClicked() {
  if (!mCurrentClient.has_value()) {
    return;
  }

  emit openClientCardRequested(mCurrentClient);
}
```

- [ ] **Step 5: Wire the signal in `main_window.cpp`**

In `src/app/main_window.cpp`'s `connectSignals()`, after the existing block:

```cpp
  connect(clientInfoPage, &ClientInfo::notesButtonClicked, clientNotesPage,
          &ClientNotesPage::setClientInfo);
  connect(clientInfoPage, &ClientInfo::notesButtonClicked, [this]() {
    setClientNavigationVisible(Pages::clientNotes, true);
    showPage(Pages::clientNotes, mBtnNotes);
  });
```

add:

```cpp
  connect(clientNotesPage, &ClientNotesPage::openClientCardRequested,
          clientCardPage, &QClientInfoCardPage::setClientInfo);
  connect(clientNotesPage, &ClientNotesPage::openClientCardRequested,
          [this]() {
            setClientNavigationVisible(Pages::clientCard, true);
            showPage(Pages::clientCard, mBtnProfile);
          });
```

- [ ] **Step 6: Build**

Run: `cmake --build build-release --target PsyClientManager --parallel`
Expected: builds cleanly (no references to the removed `mTitleLabel` remain).

- [ ] **Step 7: Run the full test suite as a regression check**

Run: `ctest --test-dir build-release --output-on-failure`
Expected: all tests pass.

- [ ] **Step 8: Commit**

```bash
git add src/pages/client_notes_page/client_notes_page.h src/pages/client_notes_page/client_notes_page.cpp src/app/main_window.cpp
git commit -m "feat(notes): add breadcrumb header and client-card navigation link"
```

---

### Task 2: Date-grouped feed dividers

**Files:**
- Modify: `src/pages/client_notes_page/client_notes_page.h`
- Modify: `src/pages/client_notes_page/client_notes_page.cpp`

**Interfaces:**
- Produces: `void ClientNotesPage::addDateDivider(const QDate &date);` — private helper used only by `reloadNotes` (Task 5 will extend this same loop).

- [ ] **Step 1: Declare the helper**

In `src/pages/client_notes_page/client_notes_page.h`, after `void addNoteBubble(const DuckClientNote &note);`:

```cpp
  void addNoteBubble(const DuckClientNote &note);
  void addDateDivider(const QDate &date);
```

- [ ] **Step 2: Add `#include <QLocale>`**

In `src/pages/client_notes_page/client_notes_page.cpp`, add to the include block (alphabetical position, after `#include <QListWidgetItem>`):

```cpp
#include <QListWidgetItem>
#include <QLocale>
#include <QMimeDatabase>
```

- [ ] **Step 3: Implement `addDateDivider`**

Add after `addNoteBubble`'s closing brace (before `addAttachmentWidgets`):

```cpp
void ClientNotesPage::addDateDivider(const QDate &date) {
  auto *divider = new QLabel(mFeedWidget);
  divider->setAlignment(Qt::AlignCenter);
  divider->setText(QStringLiteral("— %1 —").arg(
      QLocale().toString(date, QLocale::LongFormat)));
  divider->setStyleSheet("color: rgba(255, 255, 255, 0.45); background: transparent;");
  mFeedLayout->insertWidget(mFeedLayout->count() - 1, divider);
}
```

- [ ] **Step 4: Insert dividers while building the feed in `reloadNotes`**

Replace:

```cpp
  mEmptyLabel->setVisible(false);
  for (const auto &note : notes) {
    addNoteBubble(note);
  }
```

with:

```cpp
  mEmptyLabel->setVisible(false);
  QDate previousDate;
  for (const auto &note : notes) {
    const auto createdAt =
        note.created_at.has_value()
            ? QDateTime::fromMSecsSinceEpoch(*note.created_at, QTimeZone::systemTimeZone())
            : QDateTime{};
    const auto noteDate = createdAt.isValid() ? createdAt.date() : QDate();
    if (noteDate.isValid() && noteDate != previousDate) {
      addDateDivider(noteDate);
      previousDate = noteDate;
    }
    addNoteBubble(note);
  }
```

- [ ] **Step 5: Build**

Run: `cmake --build build-release --target PsyClientManager --parallel`
Expected: builds cleanly.

- [ ] **Step 6: Run the full test suite as a regression check**

Run: `ctest --test-dir build-release --output-on-failure`
Expected: all tests pass.

- [ ] **Step 7: Commit**

```bash
git add src/pages/client_notes_page/client_notes_page.h src/pages/client_notes_page/client_notes_page.cpp
git commit -m "feat(notes): group the note feed with date dividers"
```

---

### Task 3: Compact attachment rendering with click-to-expand

**Files:**
- Modify: `src/pages/client_notes_page/client_notes_page.cpp`

**Interfaces:**
- Consumes: `pcm::widgets::constants::kNotesAttachmentPreviewMaxWidth/Height` (existing).
- No signature changes in this task — `addAttachmentWidgets` keeps its current parameters; Task 5 will change how it's called (attachments passed in instead of re-fetched), not this task.

- [ ] **Step 1: Replace `addAttachmentWidgets`' body**

Replace the entire function:

```cpp
void ClientNotesPage::addAttachmentWidgets(
    QVBoxLayout *layout, const std::vector<DuckClientNoteAttachment> &attachments) {
  for (const auto &attachment : attachments) {
    const auto relativePath =
        QString::fromStdString(attachment.relative_path.value_or(""));
    if (relativePath.isEmpty()) {
      continue;
    }

    const auto absolutePath =
        QDir(pcm::app_settings::attachmentsStorageRoot()).filePath(relativePath);
    const auto fileName =
        QString::fromStdString(attachment.file_name.value_or(""));
    const auto mimeType =
        QString::fromStdString(attachment.mime_type.value_or(""));
    const auto isImage = mimeType.startsWith("image/");

    if (isImage) {
      QImageReader imageReader(absolutePath);
      imageReader.setAutoTransform(true);
      const auto image = imageReader.read();
      if (!image.isNull()) {
        auto *imageLabel = new QLabel();
        imageLabel->setPixmap(QPixmap::fromImage(image).scaled(
            pcm::widgets::constants::kNotesAttachmentPreviewMaxWidth,
            pcm::widgets::constants::kNotesAttachmentPreviewMaxHeight,
            Qt::KeepAspectRatio, Qt::SmoothTransformation));
        imageLabel->setAlignment(Qt::AlignLeft);
        imageLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
        imageLabel->setStyleSheet(
            "background: rgba(255, 255, 255, 0.02);"
            "border-radius: 10px;");
        layout->addWidget(imageLabel);
      }
    }

    auto *button = new QPushButton(
        isImage ? tr("Open image: %1").arg(fileName)
                : tr("Open file: %1").arg(fileName));
    button->setCursor(Qt::PointingHandCursor);
    button->setFlat(true);
    button->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    button->setStyleSheet(
        "QPushButton {"
        " text-align: left;"
        " color: rgba(255, 255, 255, 0.84);"
        " background: rgba(255, 255, 255, 0.04);"
        " border: 1px solid rgba(255, 255, 255, 0.08);"
        " border-radius: 10px;"
        " padding: 8px 12px;"
        "}"
        "QPushButton:hover {"
        " background: rgba(255, 255, 255, 0.07);"
        "}");
    connect(button, &QPushButton::clicked, this, [absolutePath]() {
      QDesktopServices::openUrl(QUrl::fromLocalFile(absolutePath));
    });
    layout->addWidget(button, 0, Qt::AlignLeft);
  }
}
```

with:

```cpp
void ClientNotesPage::addAttachmentWidgets(
    QVBoxLayout *layout, const std::vector<DuckClientNoteAttachment> &attachments) {
  for (const auto &attachment : attachments) {
    const auto relativePath =
        QString::fromStdString(attachment.relative_path.value_or(""));
    if (relativePath.isEmpty()) {
      continue;
    }

    const auto absolutePath =
        QDir(pcm::app_settings::attachmentsStorageRoot()).filePath(relativePath);
    const auto fileName =
        QString::fromStdString(attachment.file_name.value_or(""));
    const auto mimeType =
        QString::fromStdString(attachment.mime_type.value_or(""));
    const auto isImage = mimeType.startsWith("image/");
    const auto sizeText =
        QLocale().formattedDataSize(attachment.size_bytes.value_or(0));

    auto *row = new QWidget();
    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    auto *nameButton = new QPushButton(
        QString("%1 · %2 · %3")
            .arg(isImage ? tr("Image") : tr("File"), fileName, sizeText));
    nameButton->setFlat(true);
    nameButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    nameButton->setStyleSheet(
        "QPushButton {"
        " text-align: left;"
        " color: rgba(255, 255, 255, 0.84);"
        " background: rgba(255, 255, 255, 0.04);"
        " border: 1px solid rgba(255, 255, 255, 0.08);"
        " border-radius: 10px;"
        " padding: 8px 12px;"
        "}"
        "QPushButton:hover {"
        " background: rgba(255, 255, 255, 0.07);"
        "}");

    auto *openButton = new QPushButton(tr("Open"));
    openButton->setCursor(Qt::PointingHandCursor);
    openButton->setFlat(true);
    openButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    connect(openButton, &QPushButton::clicked, this, [absolutePath]() {
      QDesktopServices::openUrl(QUrl::fromLocalFile(absolutePath));
    });

    rowLayout->addWidget(nameButton, 0, Qt::AlignLeft);
    rowLayout->addWidget(openButton, 0, Qt::AlignLeft);
    rowLayout->addStretch();
    layout->addWidget(row);

    if (isImage) {
      nameButton->setCursor(Qt::PointingHandCursor);
      auto *previewLabel = new QLabel();
      previewLabel->setVisible(false);
      previewLabel->setAlignment(Qt::AlignLeft);
      previewLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);
      previewLabel->setStyleSheet(
          "background: rgba(255, 255, 255, 0.02);"
          "border-radius: 10px;");
      layout->addWidget(previewLabel);

      connect(nameButton, &QPushButton::clicked, this,
              [previewLabel, absolutePath]() {
                if (previewLabel->pixmap().isNull()) {
                  QImageReader imageReader(absolutePath);
                  imageReader.setAutoTransform(true);
                  const auto image = imageReader.read();
                  if (!image.isNull()) {
                    previewLabel->setPixmap(QPixmap::fromImage(image).scaled(
                        pcm::widgets::constants::kNotesAttachmentPreviewMaxWidth,
                        pcm::widgets::constants::kNotesAttachmentPreviewMaxHeight,
                        Qt::KeepAspectRatio, Qt::SmoothTransformation));
                  }
                }
                previewLabel->setVisible(!previewLabel->isVisible());
              });
    } else {
      nameButton->setEnabled(false);
    }
  }
}
```

- [ ] **Step 2: Build**

Run: `cmake --build build-release --target PsyClientManager --parallel`
Expected: builds cleanly.

- [ ] **Step 3: Run the full test suite as a regression check**

Run: `ctest --test-dir build-release --output-on-failure`
Expected: all tests pass.

- [ ] **Step 4: Commit**

```bash
git add src/pages/client_notes_page/client_notes_page.cpp
git commit -m "feat(notes): render attachments as compact rows that expand on click"
```

---

### Task 4: Composer polish — Ctrl+Enter and save confirmation

**Files:**
- Modify: `src/pages/client_notes_page/client_notes_page.h`
- Modify: `src/pages/client_notes_page/client_notes_page.cpp`

**Interfaces:**
- Produces: `bool ClientNotesPage::eventFilter(QObject *watched, QEvent *event) override;` installed on `mComposer`.

- [ ] **Step 1: Declare the event filter override and the status label member**

In `src/pages/client_notes_page/client_notes_page.h`, add a `protected:` section right before `private:`:

```cpp
protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
```

Add the member after `QPlainTextEdit *mComposer = nullptr;`:

```cpp
  QPlainTextEdit *mComposer = nullptr;
  QLabel *mSaveStatusLabel = nullptr;
```

- [ ] **Step 2: Add `#include <QKeyEvent>` and `#include <QTimer>`**

In `src/pages/client_notes_page/client_notes_page.cpp`, add to the include block:

```cpp
#include <QImageReader>
#include <QKeyEvent>
#include <QListWidgetItem>
#include <QLocale>
#include <QMimeDatabase>
#include <QPixmap>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QTimeZone>
#include <QUrl>
```

- [ ] **Step 3: Create the status label and install the event filter in `buildUi`**

Replace:

```cpp
  mComposer = new QPlainTextEdit(composerSurface);
  mComposer->setPlaceholderText(tr("Write a note in Markdown..."));
  mComposer->setMinimumHeight(120);
```

with:

```cpp
  mComposer = new QPlainTextEdit(composerSurface);
  mComposer->setPlaceholderText(tr("Write a note in Markdown..."));
  mComposer->setMinimumHeight(120);
  mComposer->installEventFilter(this);

  mSaveStatusLabel = new QLabel(composerSurface);
  mSaveStatusLabel->setStyleSheet("color: rgba(120, 220, 150, 0.9);");
  mSaveStatusLabel->setVisible(false);
```

Then, right after `composerLayout->addWidget(mComposer);`, add:

```cpp
  composerLayout->addWidget(mComposer);
  composerLayout->addWidget(mSaveStatusLabel);
  composerLayout->addWidget(mPendingAttachmentsList);
```

(This replaces the existing `composerLayout->addWidget(mComposer); composerLayout->addWidget(mPendingAttachmentsList);` pair — insert `mSaveStatusLabel` between them.)

- [ ] **Step 4: Show the confirmation after a successful save**

In `onAddNoteClicked`, replace:

```cpp
  persistPendingAttachments(newNoteId);
  mComposer->clear();
  mPendingAttachments.clear();
  refreshPendingAttachments();
  reloadNotes();
```

with:

```cpp
  persistPendingAttachments(newNoteId);
  mComposer->clear();
  mPendingAttachments.clear();
  refreshPendingAttachments();
  reloadNotes();

  mSaveStatusLabel->setText(tr("Note saved"));
  mSaveStatusLabel->setVisible(true);
  QTimer::singleShot(2000, this, [this]() { mSaveStatusLabel->setVisible(false); });
```

- [ ] **Step 5: Implement `eventFilter`**

Add after `onOpenClientCardClicked`'s closing brace (from Task 1) — or, if Task 1 hasn't been kept in memory, after `onPendingAttachmentActivated`'s closing brace:

```cpp
bool ClientNotesPage::eventFilter(QObject *watched, QEvent *event) {
  if (watched == mComposer && event->type() == QEvent::KeyPress) {
    auto *keyEvent = static_cast<QKeyEvent *>(event);
    if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
        keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
      onAddNoteClicked();
      return true;
    }
  }

  return QWidget::eventFilter(watched, event);
}
```

- [ ] **Step 6: Build**

Run: `cmake --build build-release --target PsyClientManager --parallel`
Expected: builds cleanly.

- [ ] **Step 7: Run the full test suite as a regression check**

Run: `ctest --test-dir build-release --output-on-failure`
Expected: all tests pass.

- [ ] **Step 8: Commit**

```bash
git add src/pages/client_notes_page/client_notes_page.h src/pages/client_notes_page/client_notes_page.cpp
git commit -m "feat(notes): add Ctrl+Enter shortcut and a save confirmation to the composer"
```

---

### Task 5: Feed filter — Все / С вложениями

**Files:**
- Modify: `src/pages/client_notes_page/client_notes_page.h`
- Modify: `src/pages/client_notes_page/client_notes_page.cpp`

**Interfaces:**
- Consumes: `Database::get_note_attachments` (existing), `addNoteBubble` (Task 2/3 state).
- Produces: changes `addNoteBubble`'s signature to accept pre-fetched attachments, avoiding a duplicate DB call now that `reloadNotes` needs to inspect attachments to filter.

- [ ] **Step 1: Add the combo box, filter state, and slot declarations**

In `src/pages/client_notes_page/client_notes_page.h`, add `#include <QComboBox>` to the include block, keeping alphabetical order (it sorts before `QDateTime`):

```cpp
#include "database.h"
#include <QComboBox>
#include <QDateTime>
#include <QLabel>
```

Add the slot after `onOpenClientCardClicked();`:

```cpp
  void onOpenClientCardClicked();
  void onFeedFilterChanged(int index);
```

Change the `addNoteBubble` declaration:

```cpp
  void addNoteBubble(const DuckClientNote &note);
```

to:

```cpp
  void addNoteBubble(const DuckClientNote &note,
                     const std::vector<DuckClientNoteAttachment> &attachments);
```

Add members after `QLabel *mEmptyLabel = nullptr;`:

```cpp
  QLabel *mEmptyLabel = nullptr;
  QComboBox *mFeedFilterCombo = nullptr;
  bool mAttachmentsOnlyFilter = false;
```

- [ ] **Step 2: Build the filter row above the feed in `buildUi`**

Replace:

```cpp
  auto *feedSurface = makeSurface(this);
  auto *feedSurfaceLayout = new QVBoxLayout(feedSurface);
  feedSurfaceLayout->setContentsMargins(0, 0, 0, 0);
  feedSurfaceLayout->setSpacing(0);

  mScrollArea = new QScrollArea(feedSurface);
```

with:

```cpp
  auto *feedSurface = makeSurface(this);
  auto *feedSurfaceLayout = new QVBoxLayout(feedSurface);
  feedSurfaceLayout->setContentsMargins(0, 0, 0, 0);
  feedSurfaceLayout->setSpacing(0);

  auto *filterRow = new QWidget(feedSurface);
  auto *filterRowLayout = new QHBoxLayout(filterRow);
  filterRowLayout->setContentsMargins(16, 10, 16, 10);
  filterRowLayout->setSpacing(8);
  mFeedFilterCombo = new QComboBox(filterRow);
  mFeedFilterCombo->addItem(tr("All"), false);
  mFeedFilterCombo->addItem(tr("With attachments"), true);
  filterRowLayout->addWidget(mFeedFilterCombo);
  filterRowLayout->addStretch();
  feedSurfaceLayout->addWidget(filterRow);

  mScrollArea = new QScrollArea(feedSurface);
```

At the end of `buildUi`, after the `mOpenClientCardButton` connect added in Task 1, add:

```cpp
  connect(mOpenClientCardButton, &QPushButton::clicked, this,
          &ClientNotesPage::onOpenClientCardClicked);
  connect(mFeedFilterCombo, &QComboBox::currentIndexChanged, this,
          &ClientNotesPage::onFeedFilterChanged);
```

- [ ] **Step 3: Implement `onFeedFilterChanged`**

Add after `onOpenClientCardClicked`'s closing brace:

```cpp
void ClientNotesPage::onFeedFilterChanged(const int index) {
  mAttachmentsOnlyFilter = mFeedFilterCombo->itemData(index).toBool();
  reloadNotes();
}
```

- [ ] **Step 4: Fetch attachments once per note, filter, and pass them into `addNoteBubble`**

Replace the loop built in Task 2:

```cpp
  mEmptyLabel->setVisible(false);
  QDate previousDate;
  for (const auto &note : notes) {
    const auto createdAt =
        note.created_at.has_value()
            ? QDateTime::fromMSecsSinceEpoch(*note.created_at, QTimeZone::systemTimeZone())
            : QDateTime{};
    const auto noteDate = createdAt.isValid() ? createdAt.date() : QDate();
    if (noteDate.isValid() && noteDate != previousDate) {
      addDateDivider(noteDate);
      previousDate = noteDate;
    }
    addNoteBubble(note);
  }

  QMetaObject::invokeMethod(
      mScrollArea->verticalScrollBar(), "setValue", Qt::QueuedConnection,
      Q_ARG(int, mScrollArea->verticalScrollBar()->maximum()));
```

with:

```cpp
  mEmptyLabel->setVisible(false);
  QDate previousDate;
  bool anyRendered = false;
  for (const auto &note : notes) {
    const auto attachments = mDb ? mDb->get_note_attachments(note.id)
                                 : std::vector<DuckClientNoteAttachment>{};
    if (mAttachmentsOnlyFilter && attachments.empty()) {
      continue;
    }

    const auto createdAt =
        note.created_at.has_value()
            ? QDateTime::fromMSecsSinceEpoch(*note.created_at, QTimeZone::systemTimeZone())
            : QDateTime{};
    const auto noteDate = createdAt.isValid() ? createdAt.date() : QDate();
    if (noteDate.isValid() && noteDate != previousDate) {
      addDateDivider(noteDate);
      previousDate = noteDate;
    }
    addNoteBubble(note, attachments);
    anyRendered = true;
  }

  if (!anyRendered) {
    mEmptyLabel->setText(tr("No notes match this filter"));
    mEmptyLabel->setVisible(true);
  }

  QMetaObject::invokeMethod(
      mScrollArea->verticalScrollBar(), "setValue", Qt::QueuedConnection,
      Q_ARG(int, mScrollArea->verticalScrollBar()->maximum()));
```

- [ ] **Step 5: Update `addNoteBubble` to take attachments as a parameter**

Replace the signature and the trailing attachment-fetch block:

```cpp
void ClientNotesPage::addNoteBubble(const DuckClientNote &note) {
```

with:

```cpp
void ClientNotesPage::addNoteBubble(
    const DuckClientNote &note,
    const std::vector<DuckClientNoteAttachment> &attachments) {
```

and replace:

```cpp
  if (mDb) {
    addAttachmentWidgets(layout, mDb->get_note_attachments(note.id));
  }

  mFeedLayout->insertWidget(mFeedLayout->count() - 1, bubble, 0, Qt::AlignLeft);
```

with:

```cpp
  addAttachmentWidgets(layout, attachments);

  mFeedLayout->insertWidget(mFeedLayout->count() - 1, bubble, 0, Qt::AlignLeft);
```

- [ ] **Step 6: Build**

Run: `cmake --build build-release --target PsyClientManager --parallel`
Expected: builds cleanly.

- [ ] **Step 7: Run the full test suite as a regression check**

Run: `ctest --test-dir build-release --output-on-failure`
Expected: all tests pass.

- [ ] **Step 8: Commit**

```bash
git add src/pages/client_notes_page/client_notes_page.h src/pages/client_notes_page/client_notes_page.cpp
git commit -m "feat(notes): add an attachments-only feed filter"
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
project(PsyClientManager VERSION 0.1.17 LANGUAGES CXX)
```

In `src/app/application.cpp`:

```cpp
  app.setApplicationVersion("0.1.17");
```

(If issue #21's branch has already merged to `main` and bumped to `0.1.17` by the time this task runs, use `0.1.18` instead and adjust the CHANGELOG heading to match — check the current value in `CMakeLists.txt` before editing.)

- [ ] **Step 2: Add the CHANGELOG entry**

In `CHANGELOG.md`, add above the current top-most `[0.1.1x]` entry:

```markdown
## [0.1.17] - 2026-07-30

### Added

- Notes journal: date-grouped feed, a client-card breadcrumb link, compact
  attachments that expand on click, a Ctrl+Enter composer shortcut with a
  save confirmation, and an attachments-only feed filter.
```

- [ ] **Step 3: Build and run full test suite**

Run: `cmake --build build-release --parallel && ctest --test-dir build-release --output-on-failure`
Expected: builds cleanly, all tests pass.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt src/app/application.cpp CHANGELOG.md
git commit -m "chore: bump version to 0.1.17"
```
