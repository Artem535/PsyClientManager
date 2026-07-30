#include "client_notes_page.h"

#include "../../widgets/app_settings.h"
#include "../../widgets/constants.hpp"

#include <QDesktopServices>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QImageReader>
#include <QListWidgetItem>
#include <QLocale>
#include <QMimeDatabase>
#include <QPixmap>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimeZone>
#include <QUrl>

namespace {
QFrame *makeSurface(QWidget *parent = nullptr) {
  auto *frame = new QFrame(parent);
  frame->setObjectName("notesSurface");
  const QString styleSheet = QStringLiteral(
                                  "#notesSurface {"
                                  " background: rgba(255, 255, 255, 0.05);"
                                  " border: 1px solid rgba(255, 255, 255, 0.08);"
                                  " border-radius: %1px;"
                                  "}")
                                  .arg(pcm::widgets::constants::kCardCornerRadius);
  frame->setStyleSheet(styleSheet);
  return frame;
}
} // namespace

ClientNotesPage::ClientNotesPage(std::shared_ptr<pcm::database::Database> db,
                                 QWidget *parent)
    : QWidget(parent), mDb(std::move(db)) {
  buildUi();
  reloadNotes();
}

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

void ClientNotesPage::refresh() { reloadNotes(); }

void ClientNotesPage::onAddNoteClicked() {
  if (!mDb || !mCurrentClient.has_value() || mCurrentClient->id <= 0) {
    return;
  }

  const auto markdown = mComposer->toPlainText().trimmed();
  if (markdown.isEmpty() && mPendingAttachments.isEmpty()) {
    return;
  }

  DuckClientNote note;
  note.client_id = mCurrentClient->id;
  note.body_markdown = markdown.toStdString();
  note.created_at = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
  note.updated_at = note.created_at;

  const auto newNoteId = mDb->add_client_note(note);
  if (newNoteId <= 0) {
    return;
  }

  persistPendingAttachments(newNoteId);
  mComposer->clear();
  mPendingAttachments.clear();
  refreshPendingAttachments();
  reloadNotes();
}

void ClientNotesPage::onAttachFilesClicked() {
  const auto filePaths = QFileDialog::getOpenFileNames(
      this, tr("Attach files"), QString(),
      tr("All files (*);;Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.svg)"));
  if (filePaths.isEmpty()) {
    return;
  }

  QMimeDatabase mimeDatabase;
  for (const auto &path : filePaths) {
    QFileInfo fileInfo(path);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
      continue;
    }

    const auto mimeType = mimeDatabase.mimeTypeForFile(fileInfo);
    PendingAttachment attachment;
    attachment.sourcePath = path;
    attachment.fileName = fileInfo.fileName();
    attachment.mimeType = mimeType.name();
    attachment.isImage = mimeType.name().startsWith("image/");
    mPendingAttachments.push_back(attachment);
  }

  refreshPendingAttachments();
}

void ClientNotesPage::onPendingAttachmentActivated(QListWidgetItem *item) {
  if (!item) {
    return;
  }

  const auto row = mPendingAttachmentsList->row(item);
  if (row < 0 || row >= mPendingAttachments.size()) {
    return;
  }

  mPendingAttachments.removeAt(row);
  refreshPendingAttachments();
}

void ClientNotesPage::onOpenClientCardClicked() {
  if (!mCurrentClient.has_value()) {
    return;
  }

  emit openClientCardRequested(mCurrentClient);
}

void ClientNotesPage::buildUi() {
  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(pcm::widgets::constants::kPanelPadding,
                                 pcm::widgets::constants::kPanelPadding,
                                 pcm::widgets::constants::kPanelPadding,
                                 pcm::widgets::constants::kPanelPadding);
  rootLayout->setSpacing(pcm::widgets::constants::kPanelPadding);

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

  auto *feedSurface = makeSurface(this);
  auto *feedSurfaceLayout = new QVBoxLayout(feedSurface);
  feedSurfaceLayout->setContentsMargins(0, 0, 0, 0);
  feedSurfaceLayout->setSpacing(0);

  mScrollArea = new QScrollArea(feedSurface);
  mScrollArea->setFrameShape(QFrame::NoFrame);
  mScrollArea->setWidgetResizable(true);
  mScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  mFeedWidget = new QWidget(mScrollArea);
  mFeedLayout = new QVBoxLayout(mFeedWidget);
  mFeedLayout->setContentsMargins(16, 16, 16, 16);
  mFeedLayout->setSpacing(pcm::widgets::constants::kNotesFeedItemSpacing);

  mEmptyLabel = new QLabel(tr("No notes yet"), mFeedWidget);
  mEmptyLabel->setAlignment(Qt::AlignCenter);
  mEmptyLabel->setStyleSheet("color: rgba(255, 255, 255, 0.55);");
  mFeedLayout->addWidget(mEmptyLabel);
  mFeedLayout->addStretch();

  mScrollArea->setWidget(mFeedWidget);
  feedSurfaceLayout->addWidget(mScrollArea);
  rootLayout->addWidget(feedSurface, 1);

  auto *composerSurface = makeSurface(this);
  auto *composerLayout = new QVBoxLayout(composerSurface);
  composerLayout->setContentsMargins(
      pcm::widgets::constants::kNotesHeaderHorizontalPadding,
      pcm::widgets::constants::kNotesHeaderVerticalPadding,
      pcm::widgets::constants::kNotesHeaderHorizontalPadding,
      pcm::widgets::constants::kNotesHeaderHorizontalPadding);
  composerLayout->setSpacing(pcm::widgets::constants::kNotesComposerSpacing);

  mComposer = new QPlainTextEdit(composerSurface);
  mComposer->setPlaceholderText(tr("Write a note in Markdown..."));
  mComposer->setMinimumHeight(120);

  mPendingAttachmentsList = new QListWidget(composerSurface);
  mPendingAttachmentsList->setVisible(false);
  mPendingAttachmentsList->setAlternatingRowColors(false);
  mPendingAttachmentsList->setSelectionMode(QAbstractItemView::NoSelection);
  mPendingAttachmentsList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  mPendingAttachmentsList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  mPendingAttachmentsList->setMaximumHeight(120);
  mPendingAttachmentsList->setStyleSheet(
      "QListWidget {"
      " background: rgba(255, 255, 255, 0.03);"
      " border: 1px solid rgba(255, 255, 255, 0.06);"
      " border-radius: 10px;"
      " color: rgba(255, 255, 255, 0.82);"
      "}"
      "QListWidget::item {"
      " padding: 8px 10px;"
      " border-bottom: 1px solid rgba(255, 255, 255, 0.05);"
      "}"
      "QListWidget::item:selected {"
      " background: rgba(255, 255, 255, 0.06);"
      "}");

  mAttachFilesButton = new QPushButton(tr("Attach files"), composerSurface);
  mAttachFilesButton->setCursor(Qt::PointingHandCursor);
  mAddNoteButton = new QPushButton(tr("Add note"), composerSurface);
  mAddNoteButton->setCursor(Qt::PointingHandCursor);

  composerLayout->addWidget(mComposer);
  composerLayout->addWidget(mPendingAttachmentsList);
  auto *actionsLayout = new QHBoxLayout();
  actionsLayout->setContentsMargins(0, 0, 0, 0);
  actionsLayout->setSpacing(pcm::widgets::constants::kNotesComposerSpacing);
  actionsLayout->addWidget(mAttachFilesButton, 0);
  actionsLayout->addStretch();
  actionsLayout->addWidget(mAddNoteButton, 0);
  composerLayout->addLayout(actionsLayout);
  rootLayout->addWidget(composerSurface);

  connect(mAttachFilesButton, &QPushButton::clicked, this,
          &ClientNotesPage::onAttachFilesClicked);
  connect(mAddNoteButton, &QPushButton::clicked, this,
          &ClientNotesPage::onAddNoteClicked);
  connect(mPendingAttachmentsList, &QListWidget::itemDoubleClicked, this,
          &ClientNotesPage::onPendingAttachmentActivated);
  connect(mOpenClientCardButton, &QPushButton::clicked, this,
          &ClientNotesPage::onOpenClientCardClicked);
}

void ClientNotesPage::reloadNotes() {
  clearNotes();

  if (!mCurrentClient.has_value() || mCurrentClient->id <= 0) {
    mEmptyLabel->setText(tr("Select a client to open notes."));
    mEmptyLabel->setVisible(true);
    mComposer->setEnabled(false);
    mAttachFilesButton->setEnabled(false);
    mAddNoteButton->setEnabled(false);
    mPendingAttachmentsList->setEnabled(false);
    return;
  }

  mComposer->setEnabled(true);
  mAttachFilesButton->setEnabled(true);
  mAddNoteButton->setEnabled(true);
  mPendingAttachmentsList->setEnabled(true);

  const auto notes = mDb ? mDb->get_client_notes(mCurrentClient->id)
                         : std::vector<DuckClientNote>{};
  if (notes.empty()) {
    mEmptyLabel->setText(tr("No notes yet"));
    mEmptyLabel->setVisible(true);
    return;
  }

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
}

void ClientNotesPage::clearNotes() {
  while (mFeedLayout->count() > 0) {
    auto *item = mFeedLayout->takeAt(0);
    if (item->widget()) {
      item->widget()->deleteLater();
    }
    delete item;
  }

  mEmptyLabel = new QLabel(mFeedWidget);
  mEmptyLabel->setAlignment(Qt::AlignCenter);
  mEmptyLabel->setStyleSheet("color: rgba(255, 255, 255, 0.55);");
  mFeedLayout->addWidget(mEmptyLabel);
  mFeedLayout->addStretch();
}

void ClientNotesPage::addNoteBubble(const DuckClientNote &note) {
  auto *bubble = new QFrame(mFeedWidget);
  bubble->setObjectName("noteBubble");
  bubble->setMaximumWidth(pcm::widgets::constants::kNotesBubbleMaxWidth);
  bubble->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  bubble->setStyleSheet(
      "#noteBubble {"
      " background: rgba(255, 255, 255, 0.04);"
      " border: 1px solid rgba(255, 255, 255, 0.08);"
      " border-radius: 12px;"
      "}");

  auto *layout = new QVBoxLayout(bubble);
  layout->setContentsMargins(
      pcm::widgets::constants::kNotesBubbleHorizontalPadding,
      pcm::widgets::constants::kNotesBubbleVerticalPadding,
      pcm::widgets::constants::kNotesBubbleHorizontalPadding,
      pcm::widgets::constants::kNotesBubbleVerticalPadding);
  layout->setSpacing(8);

  auto *timestampLabel = new QLabel(bubble);
  const auto createdAt =
      note.created_at.has_value()
          ? QDateTime::fromMSecsSinceEpoch(*note.created_at, QTimeZone::systemTimeZone())
          : QDateTime{};
  timestampLabel->setText(createdAt.isValid()
                              ? createdAt.toString("dd.MM.yyyy HH:mm")
                              : tr("Unknown time"));
  timestampLabel->setStyleSheet("color: rgba(255, 255, 255, 0.50);");

  auto *bodyView = new QTextBrowser(bubble);
  bodyView->setFrameShape(QFrame::NoFrame);
  bodyView->setOpenExternalLinks(true);
  bodyView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  bodyView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  bodyView->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  bodyView->setContentsMargins(0, 0, 0, 0);
  bodyView->document()->setDocumentMargin(0);
  bodyView->document()->setDefaultStyleSheet(
      "body { margin: 0px; padding: 0px; }"
      "p, ul, ol { margin-top: 0px; margin-bottom: 0px; padding-top: 0px; padding-bottom: 0px; }"
      "li { margin-top: 0px; margin-bottom: 0px; }");
  auto textOption = bodyView->document()->defaultTextOption();
  textOption.setWrapMode(QTextOption::WrapAnywhere);
  bodyView->document()->setDefaultTextOption(textOption);
  bodyView->setStyleSheet(
      "QTextBrowser {"
      " background: transparent;"
      " color: rgba(255, 255, 255, 0.92);"
      " border: none;"
      " padding: 0px;"
      " margin: 0px;"
      "}");

  const auto markdown = QString::fromStdString(note.body_markdown.value_or(""));
  bodyView->setMarkdown(markdown);
  bodyView->document()->setTextWidth(
      bubble->maximumWidth() - pcm::widgets::constants::kNotesDocumentWidthInset);
  bodyView->document()->adjustSize();
  bodyView->setMinimumHeight(qRound(bodyView->document()->size().height()) + 6);
  bodyView->setMaximumHeight(
      qRound(bodyView->document()->size().height()) +
      pcm::widgets::constants::kNotesBodyHeightExtra);

  layout->addWidget(timestampLabel);
  if (!markdown.trimmed().isEmpty()) {
    layout->addWidget(bodyView);
  } else {
    bodyView->deleteLater();
  }

  if (mDb) {
    addAttachmentWidgets(layout, mDb->get_note_attachments(note.id));
  }

  mFeedLayout->insertWidget(mFeedLayout->count() - 1, bubble, 0, Qt::AlignLeft);
}

void ClientNotesPage::addDateDivider(const QDate &date) {
  auto *divider = new QLabel(mFeedWidget);
  divider->setAlignment(Qt::AlignCenter);
  divider->setText(QStringLiteral("— %1 —").arg(
      QLocale().toString(date, QLocale::LongFormat)));
  divider->setStyleSheet("color: rgba(255, 255, 255, 0.45); background: transparent;");
  mFeedLayout->insertWidget(mFeedLayout->count() - 1, divider);
}

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

void ClientNotesPage::refreshPendingAttachments() {
  mPendingAttachmentsList->clear();
  for (const auto &attachment : mPendingAttachments) {
    const auto label = attachment.isImage
                           ? tr("Image: %1").arg(attachment.fileName)
                           : tr("File: %1").arg(attachment.fileName);
    mPendingAttachmentsList->addItem(
        tr("%1  •  Double-click to remove").arg(label));
  }

  mPendingAttachmentsList->setVisible(!mPendingAttachments.isEmpty());
}

QString ClientNotesPage::relativeNoteAttachmentPath(const int64_t clientId,
                                                    const int64_t noteId,
                                                    const QString &fileName) const {
  return QString("%1/%2/%3")
      .arg(clientId)
      .arg(noteId)
      .arg(fileName);
}

bool ClientNotesPage::persistPendingAttachments(const int64_t noteId) {
  if (!mCurrentClient.has_value() || noteId <= 0) {
    return false;
  }

  const auto rootPath = pcm::app_settings::attachmentsStorageRoot();
  QDir rootDir(rootPath);
  if (!rootDir.mkpath(QStringLiteral("."))) {
    return false;
  }

  bool allSaved = true;
  for (const auto &pending : mPendingAttachments) {
    QFileInfo sourceInfo(pending.sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile()) {
      allSaved = false;
      continue;
    }

    const auto relativePath =
        relativeNoteAttachmentPath(mCurrentClient->id, noteId, pending.fileName);
    const auto absolutePath = rootDir.filePath(relativePath);
    QFileInfo targetInfo(absolutePath);
    QDir().mkpath(targetInfo.path());
    QFile::remove(absolutePath);

    if (!QFile::copy(pending.sourcePath, absolutePath)) {
      allSaved = false;
      continue;
    }

    DuckClientNoteAttachment attachment;
    attachment.note_id = noteId;
    attachment.file_name = pending.fileName.toStdString();
    attachment.relative_path = relativePath.toStdString();
    attachment.mime_type = pending.mimeType.toStdString();
    attachment.size_bytes = sourceInfo.size();
    attachment.created_at = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
    if (mDb->add_client_note_attachment(attachment) <= 0) {
      allSaved = false;
    }
  }

  return allSaved;
}

QString ClientNotesPage::currentClientTitle() const {
  if (!mCurrentClient.has_value()) {
    return tr("No client selected");
  }

  const auto firstName = QString::fromStdString(mCurrentClient->name.value_or(""));
  const auto lastName =
      QString::fromStdString(mCurrentClient->last_name.value_or(""));
  const auto fullName = QString("%1 %2").arg(firstName, lastName).trimmed();
  return fullName.isEmpty() ? tr("Unnamed client") : fullName;
}
