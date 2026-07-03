#include "recurrence_utils.h"

#include <libical/ical.h>

#include <QTimeZone>
#include <algorithm>
#include <cstdlib>

namespace {

QString weekdayToken(const int dayOfWeek) {
  switch (dayOfWeek) {
  case 1:
    return QStringLiteral("MO");
  case 2:
    return QStringLiteral("TU");
  case 3:
    return QStringLiteral("WE");
  case 4:
    return QStringLiteral("TH");
  case 5:
    return QStringLiteral("FR");
  case 6:
    return QStringLiteral("SA");
  case 7:
  default:
    return QStringLiteral("SU");
  }
}

icaltimetype toIcalFloatingTime(const QDateTime &dateTime) {
  const auto local = dateTime.toLocalTime();
  return icaltime_from_string(local.toString(QStringLiteral("yyyyMMdd'T'HHmmss"))
                                  .toLatin1()
                                  .constData());
}

QDateTime fromIcalFloatingTime(const icaltimetype &time) {
  return QDateTime(QDate(time.year, time.month, time.day),
                   QTime(time.hour, time.minute, time.second),
                   QTimeZone::systemTimeZone());
}

} // namespace

namespace pcm::recurrence {

QString weeklyRuleForDate(const QDate &date, const int intervalWeeks) {
  return QStringLiteral("FREQ=WEEKLY;INTERVAL=%1;BYDAY=%2")
      .arg(std::max(1, intervalWeeks))
      .arg(weekdayToken(date.dayOfWeek()));
}

QVector<QDateTime> occurrences(const DuckEventSeries &series,
                               const QDateTime &rangeStart,
                               const QDateTime &rangeEnd) {
  QVector<QDateTime> occurrences;
  if (!series.start_date.has_value() || series.recurrence_rule.empty() ||
      !rangeStart.isValid() || !rangeEnd.isValid() || rangeEnd < rangeStart) {
    return occurrences;
  }

  const auto localTz = QTimeZone::systemTimeZone();
  const auto seriesStart =
      QDateTime::fromMSecsSinceEpoch(*series.start_date, QTimeZone::UTC).toTimeZone(localTz);
  if (!seriesStart.isValid()) {
    return occurrences;
  }

  if (series.recurrence_until.has_value() &&
      QDateTime::fromMSecsSinceEpoch(*series.recurrence_until, QTimeZone::UTC)
              .toTimeZone(localTz) < rangeStart) {
    return occurrences;
  }

  auto rule = icalrecurrencetype_from_string(series.recurrence_rule.c_str());
  if (rule.freq == ICAL_NO_RECURRENCE) {
    std::free(rule.rscale);
    return occurrences;
  }

  auto *iterator = icalrecur_iterator_new(rule, toIcalFloatingTime(seriesStart));
  if (!iterator) {
    std::free(rule.rscale);
    return occurrences;
  }

  constexpr int kMaxOccurrencesPerRange = 10000;
  for (int i = 0; i < kMaxOccurrencesPerRange; ++i) {
    const auto next = icalrecur_iterator_next(iterator);
    if (icaltime_is_null_time(next)) {
      break;
    }

    const auto occurrence = fromIcalFloatingTime(next);
    if (!occurrence.isValid()) {
      continue;
    }
    if (occurrence > rangeEnd) {
      break;
    }
    if (occurrence >= rangeStart) {
      occurrences.append(occurrence);
    }
  }

  icalrecur_iterator_free(iterator);
  std::free(rule.rscale);
  return occurrences;
}

DuckEvent buildVirtualOccurrence(const DuckEventSeries &series,
                                 const QDateTime &occurrenceStart,
                                 const int64_t virtualId) {
  DuckEvent event;
  event.id = virtualId;
  event.name = series.name;
  event.description = series.description;
  event.client_name = series.client_name;
  event.is_work_event = series.is_work_event;
  event.event_stat_id = series.event_stat_id;
  event.payment_stat_id = series.payment_stat_id;
  event.start_date = occurrenceStart.toUTC().toMSecsSinceEpoch();
  const auto durationMs =
      series.duration.value_or(3600) * 1000;
  event.end_date = *event.start_date + durationMs;
  event.duration = series.duration;
  event.cost = series.cost;
  event.is_online = series.is_online;
  event.meeting_url = series.meeting_url;
  event.series_id = series.id;
  event.original_occurrence_start = event.start_date;
  event.is_virtual_occurrence = true;
  return event;
}

} // namespace pcm::recurrence
