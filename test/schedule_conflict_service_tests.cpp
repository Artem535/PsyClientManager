#include <gtest/gtest.h>

#include "schedule_conflict_service.h"

namespace {
DuckEvent eventWithRange(const int64_t id, const int64_t start, const int64_t end) {
  DuckEvent event;
  event.id = id;
  event.start_date = start;
  event.end_date = end;
  return event;
}
} // namespace

TEST(ScheduleConflictServiceTest, DetectsOverlappingRanges) {
  const auto candidate = eventWithRange(2, 1'500, 2'500);
  const auto existing = eventWithRange(1, 1'000, 2'000);

  EXPECT_TRUE(pcm::schedule::hasConflict(candidate, {existing}));
}

TEST(ScheduleConflictServiceTest, TreatsAdjacentRangesAsNonOverlapping) {
  const auto candidate = eventWithRange(2, 2'000, 3'000);
  const auto existing = eventWithRange(1, 1'000, 2'000);

  EXPECT_FALSE(pcm::schedule::hasConflict(candidate, {existing}));
}

TEST(ScheduleConflictServiceTest, CandidateBeforeBufferBlocksAdjacentEvent) {
  auto candidate = eventWithRange(2, 2'000, 3'000);
  const auto existing = eventWithRange(1, 1'000, 2'000);
  candidate.buffer_before_minutes = 1;

  EXPECT_TRUE(pcm::schedule::hasConflict(candidate, {existing}));
}

TEST(ScheduleConflictServiceTest, ExistingAfterBufferBlocksAdjacentEvent) {
  const auto candidate = eventWithRange(2, 2'000, 3'000);
  auto existing = eventWithRange(1, 1'000, 2'000);
  existing.buffer_after_minutes = 1;

  EXPECT_TRUE(pcm::schedule::hasConflict(candidate, {existing}));
}

TEST(ScheduleConflictServiceTest, IgnoresEventsWithoutCompleteTimeRange) {
  const auto candidate = eventWithRange(2, 1'500, 2'500);
  auto existing = eventWithRange(1, 1'000, 2'000);
  existing.end_date.reset();

  EXPECT_FALSE(pcm::schedule::hasConflict(candidate, {existing}));
}

TEST(ScheduleConflictServiceTest, IgnoresCandidateDuringUpdate) {
  const auto candidate = eventWithRange(7, 1'500, 2'500);

  EXPECT_FALSE(pcm::schedule::hasConflict(candidate, {candidate}));
}

TEST(ScheduleConflictServiceTest, IgnoresMaterializedVersionOfSameOccurrence) {
  auto candidate = eventWithRange(8, 1'500, 2'500);
  auto materialized = eventWithRange(9, 1'000, 3'000);
  candidate.series_id = 42;
  candidate.original_occurrence_start = 1'500;
  materialized.series_id = 42;
  materialized.original_occurrence_start = 1'500;

  EXPECT_FALSE(pcm::schedule::hasConflict(candidate, {materialized}));
}

TEST(ScheduleConflictServiceTest, FindConflictReturnsTheOverlappingEvent) {
  const auto candidate = eventWithRange(2, 1'500, 2'500);
  const auto existing = eventWithRange(1, 1'000, 2'000);

  const auto conflict = pcm::schedule::findConflict(candidate, {existing});
  ASSERT_TRUE(conflict.has_value());
  EXPECT_EQ(conflict->id, 1);
}

TEST(ScheduleConflictServiceTest, FindConflictReturnsNulloptWhenNoOverlap) {
  const auto candidate = eventWithRange(2, 2'000, 3'000);
  const auto existing = eventWithRange(1, 1'000, 2'000);

  EXPECT_FALSE(pcm::schedule::findConflict(candidate, {existing}).has_value());
}

TEST(ScheduleConflictServiceTest, FindConflictReturnsFirstOverlapInEventOrder) {
  const auto candidate = eventWithRange(3, 1'500, 2'500);
  const auto first = eventWithRange(1, 1'000, 2'000);
  const auto second = eventWithRange(2, 1'800, 2'800);

  const auto conflict = pcm::schedule::findConflict(candidate, {first, second});
  ASSERT_TRUE(conflict.has_value());
  EXPECT_EQ(conflict->id, 1);
}

TEST(ScheduleConflictServiceTest, FindConflictSkipsSelfAndSameOccurrenceJustLikeHasConflict) {
  auto candidate = eventWithRange(8, 1'500, 2'500);
  auto materialized = eventWithRange(9, 1'000, 3'000);
  candidate.series_id = 42;
  candidate.original_occurrence_start = 1'500;
  materialized.series_id = 42;
  materialized.original_occurrence_start = 1'500;

  EXPECT_FALSE(pcm::schedule::findConflict(candidate, {candidate, materialized}).has_value());
}
