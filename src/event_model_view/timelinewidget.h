#pragma once
#include "eventview.h"
#include <QObject>
#include <QVBoxLayout>
#include <QWidget>
#include <memory>
#include <qboxlayout.h>

class TimelineWidget : public QWidget {
  Q_OBJECT
public:
  TimelineWidget(QWidget *parent = nullptr);
  ~TimelineWidget();

private:
  QVBoxLayout *mLayout;
  EventView *mEventView;
};