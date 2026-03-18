// src/timeline_widget/timeline_model.cpp
#include "qtimeline_model.h"
#include <QDateTime>
#include <QTimeZone>

namespace {
QString fullClientName(const DuckClient &client) {
  const auto firstName = QString::fromStdString(client.name.value_or(""));
  const auto lastName = QString::fromStdString(client.last_name.value_or(""));
  return QString("%1 %2").arg(firstName, lastName).trimmed();
}
} // namespace

QTimelineModel::QTimelineModel(
    const std::shared_ptr<pcm::database::Database> &db, QObject *parent)
    : QAbstractItemModel(parent), mDb(db) {}

QModelIndex QTimelineModel::index(int row, int column,
                                  const QModelIndex &parent) const {
  if (!parent.isValid() && row >= 0 && row < mEvents.size() && column == 0)
    return createIndex(row, column);
  return {};
}

QModelIndex QTimelineModel::parent(const QModelIndex &child) const {
  Q_UNUSED(child)
  return {};
}

int QTimelineModel::rowCount(const QModelIndex &parent) const {
  const auto rowCount = parent.isValid() ? 0 : mEvents.size();
  return rowCount;
}

int QTimelineModel::columnCount(const QModelIndex &parent) const {
  Q_UNUSED(parent)
  return 1;
}

QVariant QTimelineModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= mEvents.size())
    return {};

  const auto &event = mEvents.at(index.row());
  switch (role) {
  case IdRole:
    return static_cast<qint64>(event.id);
  case TitleRole:
    return QString::fromStdString(event.name.value_or("Undefined"));
  case DescriptionRole:
    return QString::fromStdString(event.description.value_or("Undefined"));
  case IsWorkRole:
    return event.is_work_event;
  case StartDateTimeRole:
    return QDateTime::fromMSecsSinceEpoch(event.start_date.value_or(0),
                                          QTimeZone::UTC)
        .toLocalTime();
  case EndDateTimeRole:
    return QDateTime::fromMSecsSinceEpoch(event.end_date.value_or(0),
                                          QTimeZone::UTC)
        .toLocalTime();
  case DurationRole:
    return QVariant::fromValue(event.duration);
  case EventDataRole:
    return QVariant::fromValue(event);
  default:
    return {};
  }
}

QHash<int, QByteArray> QTimelineModel::roleNames() const {
  return {{IdRole, "eventId"},
          {TitleRole, "title"},
          {DescriptionRole, "description"},
          {IsWorkRole, "isWork"},
          {StartDateTimeRole, "startDateTime"},
          {EndDateTimeRole, "endDateTime"},
          {DurationRole, "duration"},
          {EventDataRole, "eventData"}};
}

void QTimelineModel::loadEventsForDay(const QDate &date) {
  beginResetModel();
  mEvents.clear();
  mCurrentDate = date;

  const auto localTz = QTimeZone::systemTimeZone();
  const auto dayStartMs =
      QDateTime(date, QTime(0, 0), localTz).toMSecsSinceEpoch();
  const auto dayEndMs =
      QDateTime(date.addDays(1), QTime(0, 0), localTz).toMSecsSinceEpoch() - 1;

  const auto events = mDb->get_day_events(dayStartMs, dayEndMs);
  mEvents = std::move(QVector<DuckEvent>(events.begin(), events.end()));
  for (auto &event : mEvents) {
    if (!event.is_work_event) {
      continue;
    }

    try {
      const auto client = mDb->get_client_by_event(event.id);
      const auto displayName = fullClientName(client);
      if (!displayName.isEmpty()) {
        event.client_name = displayName.toStdString();
      }
    } catch (const std::exception &) {
      event.client_name = std::nullopt;
    }
  }
  qDebug() << "QTimelineModel::loadEventsForDay date=" << date
           << "loaded events=" << mEvents.size();

  endResetModel();
  emit eventsLoaded();
}

int64_t QTimelineModel::addEvent(const DuckEvent &event) {
  DuckEvent newEvent = event;
  newEvent.id = mDb->add_event(event); // save to DB
  if (newEvent.id <= 0) {
    return 0;
  }

  const int row = mEvents.size();
  beginInsertRows({}, row, row);
  mEvents.append(newEvent);
  endInsertRows();
  return newEvent.id;
}

void QTimelineModel::removeEvent(int64_t id) {
  for (int i = 0; i < mEvents.size(); ++i) {
    if (mEvents[i].id == id) {
      if (!mDb->remove_event(id)) {
        qWarning() << "QTimelineModel::removeEvent failed for id=" << id;
        return;
      }
      beginRemoveRows({}, i, i);
      mEvents.removeAt(i);
      endRemoveRows();
      break;
    }
  }
}

void QTimelineModel::updateEvent(const DuckEvent &event) {
  for (int i = 0; i < mEvents.size(); ++i) {
    if (mEvents[i].id == event.id) {
      if (!mDb->update_event(event)) {
        return;
      }
      mEvents[i] = event;
      emit dataChanged(
          index(i, 0, QModelIndex()), index(i, 0, QModelIndex()),
          {EventDataRole, TitleRole, StartDateTimeRole, EndDateTimeRole});
      break;
    }
  }
}

QModelIndex QTimelineModel::indexForEventId(int64_t id) const {
  for (int i = 0; i < mEvents.size(); ++i) {
    if (mEvents[i].id == id) {
      return index(i, 0, QModelIndex());
    }
  }
  return {};
}
