#pragma once
#include "database.h"
#include "eventitem.h"
#include <QGraphicsScene>
#include <QHash>
#include <QLoggingCategory>
#include <QObject>
#include <memory>
#include <qhash.h>

class QEventDataManager : public QObject {
  Q_OBJECT
public:
  explicit QEventDataManager(std::shared_ptr<pcm::database::Database> db,
                            QGraphicsScene *scene, QObject *parent = nullptr);

  void selectDay(const long long int &day);

  void loadEvents();

  void addEvent(const ObxEvent &event);
  [[nodiscard]] obx_id addEvent(QEventItem *event);

  void updateEvent(obx_id eventId, const ObxEvent &newData);
  void removeEvent(obx_id eventId);

signals:
  void eventsLoaded();
  void eventAdded(obx_id id);
  void eventUpdated(obx_id id);
  void eventRemoved(obx_id id);
  void eventSelected(QEventItem *item);
  void selectedDayChanged();

private slots:
  void onEventSelected();

private:
  std::shared_ptr<pcm::database::Database> mDb;
  QHash<obx_id, QEventItem *> mEvents;
  QGraphicsScene *mScene;
  long long int mSelectedDay = -1;

  static QEventItem *toEventItem(const ObxEvent &event);
  static ObxEvent toEvent(const QEventItem *item);
  void addEventItemToScene(QEventItem *item);
};