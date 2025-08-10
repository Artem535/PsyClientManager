#pragma once
// === Qt ===
#include <QAbstractItemModel>
#include <QCalendarWidget>
#include <QCheckBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QListWidget>
#include <QPushButton>
#include <QStandardItemModel>
#include <QWidget>
#include <QMessageBox>

// === STL ===
#include <memory>

// === Local ===
#include "database.h"
#include "eventitem.h"
#include "eventview.h"
#include "timelinewidget.h"

namespace Ui {
class EventInfo;
}

class QEventInfoPage final : public QWidget {
  Q_OBJECT

public:
  explicit QEventInfoPage(std::shared_ptr<pcm::database::Database> db,
                         QWidget *parent = nullptr);
  ~QEventInfoPage() override;

signals:
  void changedEditMode();
  void cleanUi();
  void needAddNewEvent(QEventItem *event);

private slots:
  void onEventClicked(QEventItem *event);
  void addEvent(QEventItem *event) const;
  void clearUi() const;

private:
  std::unique_ptr<Ui::EventInfo> mUi;
  QEventItem *mCurrentEvent = nullptr;
  QTimelineWidget *mTimelineWidget;
  bool mCreatedNewEvent = false;
  bool mInEditMode = false;

  void connectCalendar();
  void connectTimeline();
  void connectButtons();
  void connectButtonBox();
  void connectTimeEditors();
  void initDefaultTimes() const;
};
