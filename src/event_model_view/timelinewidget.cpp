#include <QGraphicsScene>
#include <qgraphicsscene.h>
#include "timelinewidget.h"
#include "eventview.h"
#include <memory>
#include <qboxlayout.h>

TimelineWidget::TimelineWidget(QWidget *parent) : QWidget(parent) {
  mLayout = new QVBoxLayout(this);
  setLayout(mLayout);
  mEventView = new EventView(this);
  mLayout->addWidget(mEventView);

  connect(mEventView, &EventView::eventSelected, this,
          &TimelineWidget::onEventSelected);
}

void TimelineWidget::onEventSelected(EventItem *event) {
  qInfo() << "TimelineWidget::onEventSelected| " << event;
  if (event != nullptr) {
    emit eventSelected(event);
  }
}

TimelineWidget::~TimelineWidget() = default;