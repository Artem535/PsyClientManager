#pragma once
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
  TimelineWidget(QWidget *parent = nullptr);
  ~TimelineWidget();

signals:
  void eventSelected(EventItem *event);

private slots:
  void onEventSelected(EventItem *event);

private:
  QVBoxLayout *mLayout;
  EventView *mEventView;
};