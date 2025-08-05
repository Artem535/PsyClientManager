#include "eventdatamanager.h"
#include "eventitem.h"
#include <qloggingcategory.h>

Q_LOGGING_CATEGORY(logEventDataManager, "pcm.event_data_manager")

EventDataManager::EventDataManager(std::shared_ptr<pcm::database::Database> db,
                                   QGraphicsScene *scene, QObject *parent)
    : QObject(parent), mDb(db), mScene(scene) {

  connect(this, &EventDataManager::selectedDayChanged, this,
          &EventDataManager::loadEvents);

  // Set selected day to today
  const auto today = QDateTime::currentDateTime().toSecsSinceEpoch();
  selectDay(today);
}

void EventDataManager::selectDay(const long long int &day) {
  qCDebug(logEventDataManager)
      << "EventDataManager::selectDay| Selected day changed" << day;
  mSelectedDay = day;
  emit selectedDayChanged();
}

void EventDataManager::addEvent(const Event &event) {
  mDb->add_event(event);
  const auto eventItem = toEventItem(event);
  addEventItemToScene(eventItem);
}

void EventDataManager::addEventItemToScene(EventItem *item) {
  if (mScene == nullptr)
    return;

  mScene->addItem(item);
  connect(item, &EventItem::itemSelected, this,
          &EventDataManager::onEventSelected);
}

void EventDataManager::loadEvents() {
  auto events = mDb->get_day_events(mSelectedDay);
  mScene->clear();

  for (const auto &event : events) {
    EventItem *item = toEventItem(event);
    addEventItemToScene(item);
  }

  emit eventsLoaded();
}

void EventDataManager::onEventSelected() {
  EventItem *item = qobject_cast<EventItem *>(sender());
  qCInfo(logEventDataManager) << "EventDataManager::onEventSelected| " << item;
  if (item != nullptr) {
    emit eventSelected(item);
  }
}

EventItem *EventDataManager::toEventItem(const Event &event) {
  const auto start = QDateTime::fromSecsSinceEpoch(event.start_date);
  const auto end = QDateTime::fromSecsSinceEpoch(event.end_date);
  const auto title = QString::fromStdString(event.name);
  auto res = new EventItem(event.id, title, start, end, event.is_work_event);
  return res;
}
