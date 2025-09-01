#include "timeline_widget.h"


Q_LOGGING_CATEGORY(logTimelineWidget, "pcm.timeline")

QTimelineWidget::QTimelineWidget(
    const std::shared_ptr<pcm::database::Database> &db, QWidget *parent)
    : QWidget(parent) {
  mLayout = new QVBoxLayout(this);
  setLayout(mLayout);

  mEventView = new QEventView(this);
  mDataManager = new QEventDataManager(db, mEventView->getScene(), this);

  mLayout->addWidget(mEventView);

  connect(mDataManager, &QEventDataManager::eventSelected, this,
          &QTimelineWidget::onEventSelected);

  connect(mDataManager, &QEventDataManager::eventsLoaded, mEventView,
          &QEventView::updateScene);

  connect(this, &QTimelineWidget::needSceneUpdate, mEventView,
          &QEventView::updateScene);
}

void QTimelineWidget::addEvent(const ObxEvent &event) const {
  if (mDataManager == nullptr)
    return;

  qCDebug(logTimelineWidget)
      << "TimelineWidget::addEvent| Adding event:" << event.id;
  mDataManager->addEvent(event);
}

void QTimelineWidget::onSelectedDayChanged(const QDate &date) const {
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

void QTimelineWidget::onEventSelected(QEventItem *event) {
  if (event == nullptr)
    return;

  qCInfo(logTimelineWidget)
      << "TimelineWidget::onEventSelected| " << event->getId();
  emit eventSelected(event);
}

obx_id QTimelineWidget::addEvent(QEventItem *item) const {
  if (item == nullptr)
    return 0;

  const auto id = mDataManager->addEvent(item);
  return id;
}
void QTimelineWidget::updateScene() { emit needSceneUpdate(); }

QTimelineWidget::~QTimelineWidget() = default;