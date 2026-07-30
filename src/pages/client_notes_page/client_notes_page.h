#pragma once

#include "database.h"

#include <QDateTime>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>
#include <optional>

class ClientNotesPage final : public QWidget {
  Q_OBJECT

public:
  explicit ClientNotesPage(std::shared_ptr<pcm::database::Database> db,
                           QWidget *parent = nullptr);
  ~ClientNotesPage() override = default;

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

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  struct PendingAttachment {
    QString sourcePath;
    QString fileName;
    QString mimeType;
    bool isImage = false;
  };

  void buildUi();
  void reloadNotes();
  void clearNotes();
  void addNoteBubble(const DuckClientNote &note);
  void addDateDivider(const QDate &date);
  void addAttachmentWidgets(QVBoxLayout *layout,
                            const std::vector<DuckClientNoteAttachment> &attachments);
  void refreshPendingAttachments();
  [[nodiscard]] QString relativeNoteAttachmentPath(int64_t clientId,
                                                   int64_t noteId,
                                                   const QString &fileName) const;
  bool persistPendingAttachments(int64_t noteId);
  [[nodiscard]] QString currentClientTitle() const;

  std::shared_ptr<pcm::database::Database> mDb;
  std::optional<DuckClient> mCurrentClient;
  QList<PendingAttachment> mPendingAttachments;

  QLabel *mClientNameLabel = nullptr;
  QPushButton *mOpenClientCardButton = nullptr;
  QScrollArea *mScrollArea = nullptr;
  QWidget *mFeedWidget = nullptr;
  QVBoxLayout *mFeedLayout = nullptr;
  QLabel *mEmptyLabel = nullptr;
  QPlainTextEdit *mComposer = nullptr;
  QLabel *mSaveStatusLabel = nullptr;
  QListWidget *mPendingAttachmentsList = nullptr;
  QPushButton *mAttachFilesButton = nullptr;
  QPushButton *mAddNoteButton = nullptr;
};
