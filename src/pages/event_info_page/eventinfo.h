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

class EventInfoPage : public QWidget {
  Q_OBJECT

public:
  explicit EventInfoPage(std::shared_ptr<pcm::database::Database> db,
                         QWidget *parent = nullptr);
  ~EventInfoPage() override;

signals:
  void changedEditMode();
  void cleanUi();
  void needAddNewEvent(EventItem *event);

private slots:
  void onEventClicked(EventItem *event);
  void addEvent(EventItem *event);
  void clearUi();

private:
  std::unique_ptr<Ui::EventInfo> mUi;
  EventItem *mCurrentEvent = nullptr;
  TimelineWidget *mTimelineWidget;
  bool mCreatedNewEvent = false;
  bool mInEditMode = false;

  void connectCalendar();
  void connectTimeline();
  void connectButtons();
  void connectButtonBox();
};
