#include "timelinewidget.h"
#include "eventdatamanager.h"
#include "eventview.h"
#include "scheme.obx.hpp"
#include <QGraphicsScene>
#include <memory>
#include <qboxlayout.h>
#include <qdatetime.h>
#include <qgraphicsscene.h>

Q_LOGGING_CATEGORY(logTimelineWidget, "pcm.timeline")

TimelineWidget::TimelineWidget(std::shared_ptr<pcm::database::Database> db,
                               QWidget *parent)
    : QWidget(parent) {
  mLayout = new QVBoxLayout(this);
  setLayout(mLayout);

  mEventView = new EventView(this);
  mDataManager = new EventDataManager(db, mEventView->getScene(), this);

  mLayout->addWidget(mEventView);

  connect(mDataManager, &EventDataManager::eventSelected, this,
          &TimelineWidget::onEventSelected);
}

void TimelineWidget::addEvent(const Event &event) {
  if (mDataManager == nullptr)
    return;

  qCDebug(logTimelineWidget)
      << "TimelineWidget::addEvent| Adding event:" << event.id;
  mDataManager->addEvent(event);
}

void TimelineWidget::onSelectedDayChanged(const QDate &date) {
  if (mDataManager == nullptr)
    return;

  qCDebug(logTimelineWidget)
      << "TimelineWidget::onSelectedDayChanged| Date changed:" << date;
  const auto currentTime = QTime::currentTime();
  const auto daySec = QDateTime(date, currentTime).toSecsSinceEpoch();
  mDataManager->selectDay(daySec);
}

void TimelineWidget::onEventSelected(std::shared_ptr<EventItem> event) {
  qCInfo(logTimelineWidget)
      << "TimelineWidget::onEventSelected| " << event->getId();
  if (event != nullptr) {
    emit eventSelected(event);
  }
}

void TimelineWidget::addEvent(std::shared_ptr<EventItem> item) {
  if (item != nullptr) {
    mDataManager->addEvent(item);
  }
}

TimelineWidget::~TimelineWidget() = default;