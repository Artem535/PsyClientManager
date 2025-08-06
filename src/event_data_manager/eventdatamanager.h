#pragma once
#include "database.h"
#include "eventitem.h"
#include <QGraphicsScene>
#include <QObject>
#include <memory>
#include <QLoggingCategory>
#include <QHash>
#include <qhash.h>

class EventDataManager : public QObject {
  Q_OBJECT
public:
  explicit EventDataManager(std::shared_ptr<pcm::database::Database> db,
                            QGraphicsScene *scene, QObject *parent = nullptr);

  void selectDay(const long long int &day);

  void loadEvents();

  void addEvent(const Event &event);
  void addEvent(std::shared_ptr<EventItem> event);

  void updateEvent(obx_id eventId, const Event &newData);
  void removeEvent(obx_id eventId);

signals:
  void eventsLoaded();
  void eventAdded(obx_id id);
  void eventUpdated(obx_id id);
  void eventRemoved(obx_id id);
  void eventSelected(std::shared_ptr<EventItem> item);
  void selectedDayChanged();

private slots:
  void onEventSelected();

private:
  std::shared_ptr<pcm::database::Database> mDb;
  QHash<obx_id, std::shared_ptr<EventItem>> mEvents;
  QGraphicsScene *mScene;
  long long int mSelectedDay = -1;

  std::shared_ptr<EventItem> toEventItem(const Event &event);
  Event toEvent(std::shared_ptr<EventItem> item);
  void addEventItemToScene(std::shared_ptr<EventItem> item);
};