// src/timeline_widget/timeline_model.cpp
#include "qtimeline_model.h"
#include "recurrence_utils.h"
#include <QDateTime>
#include <QTimeZone>
#include <algorithm>
#include <set>

namespace {
QString fullClientName(const DuckClient &client) {
  const auto firstName = QString::fromStdString(client.name.value_or(""));
  const auto lastName = QString::fromStdString(client.last_name.value_or(""));
  return QString("%1 %2").arg(firstName, lastName).trimmed();
}

bool sameRecurringOccurrence(const DuckEvent &left, const DuckEvent &right) {
  return left.series_id.has_value() && right.series_id.has_value() &&
         left.original_occurrence_start.has_value() &&
         right.original_occurrence_start.has_value() &&
         *left.series_id == *right.series_id &&
         *left.original_occurrence_start == *right.original_occurrence_start;
}

bool rangesOverlap(const DuckEvent &left, const DuckEvent &right) {
  if (!left.start_date.has_value() || !left.end_date.has_value() ||
      !right.start_date.has_value() || !right.end_date.has_value()) {
    return false;
  }

  return *left.start_date < *right.end_date &&
         *left.end_date > *right.start_date;
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
  std::set<std::pair<int64_t, int64_t>> materializedOccurrences;
  for (auto &event : mEvents) {
    if (event.series_id.has_value() && event.original_occurrence_start.has_value()) {
      materializedOccurrences.insert({*event.series_id, *event.original_occurrence_start});
    }
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

  const auto rangeStart = QDateTime(date, QTime(0, 0), localTz);
  const auto rangeEnd = QDateTime(date.addDays(1), QTime(0, 0), localTz).addMSecs(-1);
  const auto exceptions = mDb->get_event_series_exceptions_for_range(dayStartMs, dayEndMs);
  auto seriesList = mDb->get_event_series_for_range(dayStartMs, dayEndMs);
  for (auto &series : seriesList) {
    if (series.is_work_event && series.client_id.has_value()) {
      try {
        const auto client = mDb->get_client(*series.client_id);
        if (client) {
          const auto displayName = fullClientName(*client);
          if (!displayName.isEmpty()) {
            series.client_name = displayName.toStdString();
          }
        }
      } catch (const std::exception &) {
        series.client_name = std::nullopt;
      }
    }

    const auto occurrences = pcm::recurrence::occurrences(series, rangeStart, rangeEnd);
    for (const auto &occurrence : occurrences) {
      const auto occurrenceStartMs = occurrence.toUTC().toMSecsSinceEpoch();
      if (exceptions.contains({series.id, occurrenceStartMs}) ||
          materializedOccurrences.contains({series.id, occurrenceStartMs})) {
        continue;
      }
      const auto virtualId =
          -(series.id * 1'000'000LL + static_cast<int64_t>(occurrence.date().toJulianDay()));
      mEvents.append(pcm::recurrence::buildVirtualOccurrence(series, occurrence, virtualId));
    }
  }

  std::sort(mEvents.begin(), mEvents.end(), [](const DuckEvent &left, const DuckEvent &right) {
    return left.start_date.value_or(0) < right.start_date.value_or(0);
  });
  qDebug() << "QTimelineModel::loadEventsForDay date=" << date
           << "loaded events=" << mEvents.size();

  endResetModel();
  emit eventsLoaded();
}

int64_t QTimelineModel::addEvent(const DuckEvent &event, const bool allowOverlap) {
  DuckEvent newEvent = event;
  newEvent.id = mDb->add_event(event, allowOverlap); // save to DB
  if (newEvent.id <= 0) {
    return 0;
  }

  const int row = mEvents.size();
  beginInsertRows({}, row, row);
  mEvents.append(newEvent);
  endInsertRows();
  return newEvent.id;
}

int64_t QTimelineModel::addEventSeries(const DuckEvent &event, const int64_t clientId,
                                       const QString &recurrenceRule,
                                       const std::optional<int64_t> recurrenceUntilMs) {
  DuckEventSeries series;
  series.name = event.name;
  series.description = event.description;
  series.client_id = clientId > 0 ? std::make_optional(clientId) : std::nullopt;
  series.is_work_event = event.is_work_event;
  series.event_stat_id = event.event_stat_id;
  series.payment_stat_id = event.payment_stat_id;
  series.start_date = event.start_date;
  series.end_date = event.end_date;
  series.duration = event.duration;
  series.cost = event.cost;
  series.is_online = event.is_online;
  series.meeting_url = event.meeting_url;
  series.recurrence_rule = recurrenceRule.trimmed().toStdString();
  series.recurrence_until = recurrenceUntilMs;

  return mDb ? mDb->add_event_series(series) : 0;
}

bool QTimelineModel::updateEventSeries(const DuckEvent &event, const int64_t seriesId,
                                       const int64_t clientId,
                                       const QString &recurrenceRule,
                                       const std::optional<int64_t> recurrenceUntilMs) {
  if (!mDb || seriesId <= 0) {
    return false;
  }

  DuckEventSeries series;
  std::optional<DuckEventSeries> existingSeriesValue;
  if (const auto existingSeries = mDb->get_event_series(seriesId)) {
    series = *existingSeries;
    existingSeriesValue = *existingSeries;
  }
  series.id = seriesId;
  series.name = event.name;
  series.description = event.description;
  series.client_id = clientId > 0 ? std::make_optional(clientId) : std::nullopt;
  series.is_work_event = event.is_work_event;
  series.event_stat_id = event.event_stat_id;
  series.payment_stat_id = event.payment_stat_id;
  series.duration = event.duration;
  series.cost = event.cost;
  series.is_online = event.is_online;
  series.meeting_url = event.meeting_url;
  series.recurrence_rule = recurrenceRule.trimmed().toStdString();
  series.recurrence_until = recurrenceUntilMs;

  if (existingSeriesValue.has_value() && existingSeriesValue->start_date.has_value() &&
      event.start_date.has_value() && event.end_date.has_value()) {
    const auto localTz = QTimeZone::systemTimeZone();
    const auto originalStart =
        QDateTime::fromMSecsSinceEpoch(*existingSeriesValue->start_date, localTz);
    const auto editedStart = QDateTime::fromMSecsSinceEpoch(*event.start_date, localTz);
    const auto updatedStart =
        QDateTime(originalStart.date(), editedStart.time(), localTz).toMSecsSinceEpoch();
    const auto durationMs = *event.end_date - *event.start_date;
    series.start_date = updatedStart;
    series.end_date = updatedStart + durationMs;
  } else {
    series.start_date = event.start_date;
    series.end_date = event.end_date;
  }

  return mDb->update_event_series(series);
}

bool QTimelineModel::deactivateEventSeries(const int64_t seriesId) {
  return mDb && mDb->deactivate_event_series(seriesId);
}

bool QTimelineModel::removeFutureEventSeriesOccurrences(
    const int64_t seriesId, const int64_t occurrenceStartMs) {
  if (!mDb || seriesId <= 0 || occurrenceStartMs <= 0) {
    return false;
  }

  const auto series = mDb->get_event_series(seriesId);
  if (!series) {
    return false;
  }

  auto updatedSeries = *series;
  updatedSeries.recurrence_until = occurrenceStartMs - 1;
  if (!mDb->update_event_series(updatedSeries)) {
    return false;
  }

  return mDb->delete_event_series_overrides_from(seriesId, occurrenceStartMs);
}

std::optional<DuckEventSeries> QTimelineModel::eventSeriesById(const int64_t seriesId) const {
  if (!mDb || seriesId <= 0) {
    return std::nullopt;
  }

  const auto series = mDb->get_event_series(seriesId);
  if (!series) {
    return std::nullopt;
  }
  return *series;
}

void QTimelineModel::removeEvent(int64_t id) {
  for (int i = 0; i < mEvents.size(); ++i) {
    if (mEvents[i].id == id) {
      if (mEvents[i].series_id.has_value() &&
          mEvents[i].original_occurrence_start.has_value()) {
        if (!mDb->add_event_series_exception(*mEvents[i].series_id,
                                             *mEvents[i].original_occurrence_start,
                                             "deleted")) {
          qWarning() << "QTimelineModel::removeEvent failed to add series exception for id="
                     << id;
          return;
        }
        if (!mEvents[i].is_virtual_occurrence && !mDb->remove_event(id)) {
          qWarning() << "QTimelineModel::removeEvent failed for recurring override id="
                     << id;
          return;
        }
        beginRemoveRows({}, i, i);
        mEvents.removeAt(i);
        endRemoveRows();
        break;
      }
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

void QTimelineModel::updateEvent(const DuckEvent &event, const bool allowOverlap) {
  for (int i = 0; i < mEvents.size(); ++i) {
    if (mEvents[i].id == event.id) {
      if (!mDb->update_event(event, allowOverlap)) {
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

bool QTimelineModel::hasConflict(const DuckEvent &event) const {
  for (const auto &loadedEvent : mEvents) {
    if (loadedEvent.id == event.id || sameRecurringOccurrence(loadedEvent, event)) {
      continue;
    }

    if (rangesOverlap(event, loadedEvent)) {
      return true;
    }
  }

  return mDb && mDb->has_conflict(event);
}

const QVector<DuckEvent> &QTimelineModel::events() const {
  return mEvents;
}

QModelIndex QTimelineModel::indexForEventId(int64_t id) const {
  for (int i = 0; i < mEvents.size(); ++i) {
    if (mEvents[i].id == id) {
      return index(i, 0, QModelIndex());
    }
  }
  return {};
}
