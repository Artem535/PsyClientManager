#pragma once

#include "database.h"
#include "recurrence_utils.h"

#include <QDateTime>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QVariant>
#include <QVector>
#include <QWidget>
#include <memory>
#include <optional>

namespace oclero::qlementine {
class SegmentedControl;
}

class ClientNotesPage final : public QWidget {
  Q_OBJECT

public:
  explicit ClientNotesPage(std::shared_ptr<pcm::database::Database> db,
                           QWidget *parent = nullptr);
  ~ClientNotesPage() override = default;

signals:
  void openClientCardRequested(const std::optional<DuckClient> &client);
  void openEventRequested(int64_t eventId, qint64 dayMs);

public slots:
  void setClientInfo(const std::optional<DuckClient> &client);
  void refresh();

private slots:
  void onAddNoteClicked();
  void onAttachFilesClicked();
  void onPendingAttachmentActivated(QListWidgetItem *item);
  void onOpenClientCardClicked();
  void onFeedFilterChanged();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  enum class FeedFilter { All, Sessions, Notes };

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
  void addSessionEntry(const DuckEvent &event);
  void addDateDivider(const QDate &date);
  void addAttachmentWidgets(QVBoxLayout *layout,
                            const std::vector<DuckClientNoteAttachment> &attachments);
  void refreshPendingAttachments();
  void updateAppointmentSummary(const QVector<DuckEvent> &events);
  void onLinkSessionButtonClicked();
  void updateLinkButtonText();
  [[nodiscard]] std::optional<DuckEvent> nearestPastEvent(const QVector<DuckEvent> &events) const;
  [[nodiscard]] QString relativeNoteAttachmentPath(int64_t clientId,
                                                   int64_t noteId,
                                                   const QString &fileName) const;
  bool persistPendingAttachments(int64_t noteId);
  [[nodiscard]] QString currentClientTitle() const;

  std::shared_ptr<pcm::database::Database> mDb;
  std::optional<DuckClient> mCurrentClient;
  QList<PendingAttachment> mPendingAttachments;
  FeedFilter mFeedFilter = FeedFilter::All;
  QVector<DuckEvent> mCachedFeedEvents;
  std::optional<DuckEvent> mPendingLinkedEvent;
  bool mLinkManuallySet = false;

  QLabel *mClientNameLabel = nullptr;
  QLabel *mAppointmentSummaryLabel = nullptr;
  QPushButton *mOpenClientCardButton = nullptr;
  oclero::qlementine::SegmentedControl *mFeedFilterControl = nullptr;
  QScrollArea *mScrollArea = nullptr;
  QWidget *mFeedWidget = nullptr;
  QVBoxLayout *mFeedLayout = nullptr;
  QLabel *mEmptyLabel = nullptr;
  QPlainTextEdit *mComposer = nullptr;
  QLabel *mSaveStatusLabel = nullptr;
  QListWidget *mPendingAttachmentsList = nullptr;
  QPushButton *mAttachFilesButton = nullptr;
  QPushButton *mLinkSessionButton = nullptr;
  QPushButton *mAddNoteButton = nullptr;
};
