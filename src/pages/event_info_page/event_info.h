#pragma once

// === Qt ===
#include <QAbstractItemModel>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QStandardItemModel>
#include <QWidget>

// === STL ===
#include <memory>

// === Local ===
#include "database.h"
#include "event_item.h"
#include "timeline_widget.h"

namespace Ui {
class EventInfo;
}

class QEventInfoPage final : public QWidget {
  Q_OBJECT

public:
  /**
   * @brief Constructs a QEventInfoPage object.
   *
   * Initializes the UI, connects signals and slots, and sets up default time
   * values.
   *
   * @param db A shared pointer to the database.
   * @param parent The parent widget (optional).
   */
  explicit QEventInfoPage(const std::shared_ptr<pcm::database::Database> &db,
                          QWidget *parent = nullptr);

  /** @brief Destructor. */
  ~QEventInfoPage() override;

signals:
  void changedEditMode();
  void needAddNewEvent(QEventItem *event);
  void needSceneUpdate();

private slots:
  void onEventClicked(QEventItem *event);
  void saveEvent();

private:
  void connectCalendar();
  void connectTimeline();
  void connectButtons();
  void connectButtonBox();
  void connectTimeEditors();
  void connectSceneUpdate();
  void initDefaultTimes() const;
  void initClientComboBox() const;
  void connectEventTypes() const;
  void initDefaultSates();

  void clearUi() const;
  void addEvent(QEventItem *event) const;
  bool validateInput();
  void updateButtonState() const;

  Ui::EventInfo *mUi = nullptr;
  std::shared_ptr<pcm::database::Database> mDb;
  QTimelineWidget *mTimelineWidget = nullptr;
  QPointer<QEventItem> mCurrentEvent;
  bool mInEditMode = false;
  bool mCreatedNewEvent = false;
};