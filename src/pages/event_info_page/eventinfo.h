#pragma once
#include "database.h"
#include "eventitem.h"
#include "timelinewidget.h"
#include <QAbstractItemModel>
#include <QCheckBox>
#include <QDateTime>
#include <QListWidget>
#include <QStandardItemModel>
#include <QWidget>
#include <memory>

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

private slots:
  void onEventClicked(EventItem *event);

private:
  std::unique_ptr<Ui::EventInfo> mUi;
  TimelineWidget *mTimelineWidget;
  bool mInEditMode = false;

  void connectCalendar();
  void connectTimeline();
  void connectButtons();
  void connectButtonBox();
};
