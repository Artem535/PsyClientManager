// src/timeline_widget/timeline_model.h
#pragma once

#include <QAbstractItemModel>
#include <QDate>
#include <QVector>
#include <memory>

#include "database.h"
#include "event_item.h" // для ObxEvent

Q_DECLARE_METATYPE(ObxEvent)

class QTimelineModel final : public QAbstractItemModel {
  Q_OBJECT

public:
  enum Roles {
    IdRole = Qt::UserRole + 1,
    TitleRole,
    DescriptionRole,
    IsWorkRole,
    StartDateTimeRole,
    EndDateTimeRole,
    DurationRole,
    EventDataRole
  };

  explicit QTimelineModel(const std::shared_ptr<pcm::database::Database> &db,
                          QObject *parent = nullptr);

  QModelIndex index(int row, int column,
                    const QModelIndex &parent) const override;
  QModelIndex parent(const QModelIndex &child) const override;
  int rowCount(const QModelIndex &parent) const override;
  int columnCount(const QModelIndex &parent) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  // API
  void loadEventsForDay(const QDate &date);
  int64_t addEvent(const ObxEvent &event);
  void removeEvent(int64_t id);
  void updateEvent(const ObxEvent &event);

  QModelIndex indexForEventId(int64_t id) const;

signals:
  void eventsLoaded();

private:
  std::shared_ptr<pcm::database::Database> mDb;
  QVector<ObxEvent> mEvents;
  QDate mCurrentDate;
};