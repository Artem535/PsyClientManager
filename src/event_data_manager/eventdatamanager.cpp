#include "eventdatamanager.h"

EventDataManager::EventDataManager(std::shared_ptr<pcm::database::Database> db,
                                   QGraphicsScene *scene, QObject *parent)
    : QObject(parent), mDb(db), mScene(scene) {}

void EventDataManager::selectDay(const long long int &day) {
  mSelectedDay = day;
  loadEvents();
}

void EventDataManager::loadEvents() {
  auto events = mDb->get_day_events(mSelectedDay);

  for (const auto &event : events) {
    EventItem *item =
        new EventItem(QString::fromStdString(event.name),
                      QDateTime::fromSecsSinceEpoch(event.start_date),
                      QDateTime::fromSecsSinceEpoch(event.end_date));
    mScene->addItem(item);
  }

  emit eventsLoaded();
}
