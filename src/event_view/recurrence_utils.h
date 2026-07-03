#pragma once

#include "schema.hpp"

#include <QDateTime>
#include <QVector>

namespace pcm::recurrence {

QString weeklyRuleForDate(const QDate &date, int intervalWeeks = 1);
QVector<QDateTime> occurrences(const DuckEventSeries &series,
                               const QDateTime &rangeStart,
                               const QDateTime &rangeEnd);
DuckEvent buildVirtualOccurrence(const DuckEventSeries &series,
                                 const QDateTime &occurrenceStart,
                                 int64_t virtualId);

} // namespace pcm::recurrence
