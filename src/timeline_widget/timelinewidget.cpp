#include "timelinewidget.h"
#include "eventdatamanager.h"
#include "eventview.h"
#include "scheme.obx.hpp"
#include <QGraphicsScene>
#include <memory>
#include <qboxlayout.h>
#include <qdatetime.h>
#include <qgraphicsscene.h>
#include <qtimezone.h>

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

  connect(mDataManager, &EventDataManager::eventsLoaded, mEventView,
          &EventView::updateScene);
}

void TimelineWidget::addEvent(const Event &event) {
  if (mDataManager == nullptr)
    return;

  qCDebug(logTimelineWidget)
      << "TimelineWidget::addEvent| Adding event:" << event.id;
  mDataManager->addEvent(event);
}

void TimelineWidget::onSelectedDayChanged(const QDate &date) {
  qCDebug(logTimelineWidget)
      << "TimelineWidget::onSelectedDayChanged| Date input:" << date;

  const auto currentTime = QTime::currentTime();
  const auto day = QDateTime(date, currentTime, QTimeZone::LocalTime);
  const auto dayUTC = day.toUTC();
  const auto daySec = dayUTC.toMSecsSinceEpoch();

  qCDebug(logTimelineWidget)
      << "TimelineWidget::onSelectedDayChanged| Date output:" << daySec;

  mDataManager->selectDay(daySec);
}

void TimelineWidget::onEventSelected(EventItem *event) {
  if (event == nullptr)
    return;

  qCInfo(logTimelineWidget)
      << "TimelineWidget::onEventSelected| " << event->getId();
    emit eventSelected(event);
}

void TimelineWidget::addEvent(EventItem *item) {
  if (item != nullptr) {
    mDataManager->addEvent(item);
  }
}

TimelineWidget::~TimelineWidget() = default;