#include "schedule_conflict_service.h"

namespace pcm::schedule {
namespace {

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

  const auto leftStart = *left.start_date - left.buffer_before_minutes * 60'000;
  const auto leftEnd = *left.end_date + left.buffer_after_minutes * 60'000;
  const auto rightStart = *right.start_date - right.buffer_before_minutes * 60'000;
  const auto rightEnd = *right.end_date + right.buffer_after_minutes * 60'000;
  return leftStart < rightEnd && leftEnd > rightStart;
}

} // namespace

bool hasConflict(const DuckEvent &candidate,
                 const QVector<DuckEvent> &events) {
  for (const auto &event : events) {
    if (event.id == candidate.id || sameRecurringOccurrence(event, candidate)) {
      continue;
    }

    if (rangesOverlap(candidate, event)) {
      return true;
    }
  }

  return false;
}

} // namespace pcm::schedule
