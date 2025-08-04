#include "timelinewidget.h"
#include "eventdatamanager.h"
#include "eventview.h"
#include <QGraphicsScene>
#include <qboxlayout.h>
#include <qgraphicsscene.h>

TimelineWidget::TimelineWidget(std::shared_ptr<pcm::database::Database> db,
                               QWidget *parent)
    : QWidget(parent) {
  mLayout = new QVBoxLayout(this);
  setLayout(mLayout);

  mEventView = new EventView(this);
  mDataManager = new EventDataManager(db, mEventView->getScene(), this);

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