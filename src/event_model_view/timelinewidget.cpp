#include "timelinewidget.h"
#include "eventview.h"
#include <memory>
#include <qboxlayout.h>

TimelineWidget::TimelineWidget(QWidget *parent) : QWidget(parent) {
  mLayout = new QVBoxLayout(this);
  setLayout(mLayout);
  mEventView = new EventView(this);
  mLayout->addWidget(mEventView);
}

TimelineWidget::~TimelineWidget() = default;