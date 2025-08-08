#include "eventdatamanager.h"
#include "eventitem.h"
#include <memory>
#include <qloggingcategory.h>

Q_LOGGING_CATEGORY(logEventDataManager, "pcm.event_data_manager")

EventDataManager::EventDataManager(std::shared_ptr<pcm::database::Database> db,
                                   QGraphicsScene *scene, QObject *parent)
    : QObject(parent), mDb(db), mScene(scene) {

  connect(this, &EventDataManager::selectedDayChanged, this,
          &EventDataManager::loadEvents);

  // Set selected day to today
  const auto today = QDateTime::currentDateTime().toMSecsSinceEpoch();
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

void EventDataManager::addEvent(EventItem *item) {

  const Event event = toEvent(item);
  const obx_id id = mDb->add_event(event);
  item->setId(id);
  addEventItemToScene(item);
}

void EventDataManager::addEventItemToScene(EventItem *item) {
  if (mScene == nullptr)
    return;

  mScene->addItem(item);
  mEvents.insertOrAssign(item->getId(), item);
  connect(item, &EventItem::itemSelected, this,
          &EventDataManager::onEventSelected);
}

void EventDataManager::loadEvents() {
  auto events = mDb->get_day_events(mSelectedDay);
  mEvents.clear();
  mScene->clear();

  for (const auto &event : events) {
    auto item = toEventItem(event);
    addEventItemToScene(item);
  }

  emit eventsLoaded();
}

void EventDataManager::onEventSelected() {
  EventItem *item_tmp = qobject_cast<EventItem *>(sender());
  auto item = mEvents[item_tmp->getId()];

  qCInfo(logEventDataManager)
      << "EventDataManager::onEventSelected| " << item->getId();
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

Event EventDataManager::toEvent(EventItem *item) {
  Event res{};
  res.id = item->getId();
  res.is_work_event = item->isWorkItem();
  res.start_date = item->getStartTime().toMSecsSinceEpoch();
  res.end_date = item->getEndTime().toMSecsSinceEpoch();
  res.name = item->getTitle().toStdString();

  return res;
}