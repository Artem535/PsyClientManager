#pragma once

#include "schema.hpp"

#include <QDateTime>
#include <QVector>

#include <optional>

namespace pcm::database {
class Database;
}

namespace pcm::recurrence {

QString fullClientName(const DuckClient &client);
void resolveSeriesClientName(pcm::database::Database &db, DuckEventSeries &series);
QString weeklyRuleForDate(const QDate &date, int intervalWeeks = 1);
QVector<QDateTime> occurrences(const DuckEventSeries &series,
                               const QDateTime &rangeStart,
                               const QDateTime &rangeEnd);
DuckEvent buildVirtualOccurrence(const DuckEventSeries &series,
                                 const QDateTime &occurrenceStart,
                                 int64_t virtualId);

QVector<DuckEvent> eventsForClient(pcm::database::Database &db, int64_t clientId,
                                   const QDateTime &virtualWindowStart,
                                   const QDateTime &virtualWindowEnd);

struct LastNextAppointment {
  std::optional<DuckEvent> last;
  std::optional<DuckEvent> next;
};

LastNextAppointment lastAndNextAppointment(const QVector<DuckEvent> &events, qint64 nowMs);

} // namespace pcm::recurrence
