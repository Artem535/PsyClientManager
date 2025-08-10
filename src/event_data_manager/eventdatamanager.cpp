#include "eventdatamanager.h"
#include "eventitem.h"
#include <memory>
#include <utility>
#include <qloggingcategory.h>
#include <qtimezone.h>

Q_LOGGING_CATEGORY(logEventDataManager, "pcm.event_data_manager")

EventDataManager::EventDataManager(std::shared_ptr<pcm::database::Database> db,
                                   QGraphicsScene *scene, QObject *parent)
    : QObject(parent), mDb(std::move(db)), mScene(scene) {

  connect(this, &EventDataManager::selectedDayChanged, this,
          &EventDataManager::loadEvents);

  // Set selected day to today
  const auto today = QDateTime::currentMSecsSinceEpoch();
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
  const auto events = mDb->get_day_events(mSelectedDay);
  mEvents.clear();
  mScene->clear();

  for (const auto &event : events) {
    const auto item = toEventItem(event);
    addEventItemToScene(item);
  }

  emit eventsLoaded();
}

void EventDataManager::onEventSelected() {
  const auto item_tmp = qobject_cast<EventItem *>(sender());
  EventItem *item = mEvents[item_tmp->getId()];

  qCInfo(logEventDataManager)
      << "EventDataManager::onEventSelected| " << item->getId();
  if (item != nullptr) {
    emit eventSelected(item);
  }
}

EventItem *EventDataManager::toEventItem(const Event &event) {
  // Parse start and end times from UTC (as stored in the database)
  const QDateTime startUtc = QDateTime::fromMSecsSinceEpoch(event.start_date, Qt::UTC);
  const QDateTime endUtc = QDateTime::fromMSecsSinceEpoch(event.end_date, Qt::UTC);

  // Convert UTC times to local time for display
  const QDateTime startLocal = startUtc.toLocalTime();
  const QDateTime endLocal = endUtc.toLocalTime();

  // Convert the event name from std::string to QString
  const QString title = QString::fromStdString(event.name);

  // Create and return a new EventItem using local time
  const auto item = new EventItem(
      event.id,
      title,
      startLocal,
      endLocal,
      event.is_work_event
  );

  return item;
}

Event EventDataManager::toEvent(const EventItem *item) {
  if (!item) {
    qCWarning(logEventDataManager) << "EventDataManager::toEvent| item is null";
    return {};
  }

  Event event{};

  // Copy event ID
  event.id = item->getId();

  // Convert local time from UI to UTC for storage
  const QDateTime startUtc = item->getStartTime().toUTC();
  const QDateTime endUtc = item->getEndTime().toUTC();

  event.start_date = startUtc.toMSecsSinceEpoch(); // Store in UTC
  event.end_date = endUtc.toMSecsSinceEpoch();     // Store in UTC

  // Copy other fields
  event.is_work_event = item->isWorkItem();
  event.name = item->getTitle().toStdString();

  return event;
}