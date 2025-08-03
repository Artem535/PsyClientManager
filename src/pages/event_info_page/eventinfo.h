#pragma once
#include "config.h"
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

class EventInfo : public QWidget {
  Q_OBJECT

public:
  explicit EventInfo(QWidget *parent = nullptr);
  ~EventInfo() override;

signals:
  void changedEditMode();

private slots:
  void onEventClicked(EventItem *event);

private:
  std::unique_ptr<Ui::EventInfo> mUi;
  TimelineWidget *mTimelineWidget;
  bool mInEditMode = false;
};
