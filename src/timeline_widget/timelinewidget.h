#pragma once
#include "eventdatamanager.h"
#include "eventitem.h"
#include "eventview.h"
#include <QObject>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>
#include <qboxlayout.h>
#include <qtmetamacros.h>

class TimelineWidget : public QWidget {
  Q_OBJECT
public:
  TimelineWidget(std::shared_ptr<pcm::database::Database> db,
                 QWidget *parent = nullptr);
  ~TimelineWidget();

signals:
  void eventSelected(EventItem *event);

private slots:
  void onEventSelected(EventItem *event);

private:
  QVBoxLayout *mLayout;
  EventView *mEventView;
  EventDataManager *mDataManager;
};