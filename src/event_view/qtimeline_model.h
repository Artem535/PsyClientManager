// src/timeline_widget/timeline_model.h
#pragma once

#include <QAbstractItemModel>
#include <QDate>
#include <QVector>
#include <memory>

#include "database.h"
#include "event_item.h" // for DuckEvent

Q_DECLARE_METATYPE(DuckEvent)

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
  int64_t addEvent(const DuckEvent &event, bool allowOverlap = true);
  void removeEvent(int64_t id);
  void updateEvent(const DuckEvent &event, bool allowOverlap = true);
  bool hasConflict(const DuckEvent &event) const;
  const QVector<DuckEvent> &events() const;

  QModelIndex indexForEventId(int64_t id) const;

signals:
  void eventsLoaded();

private:
  std::shared_ptr<pcm::database::Database> mDb;
  QVector<DuckEvent> mEvents;
  QDate mCurrentDate;
};
