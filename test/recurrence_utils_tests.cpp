#include <Poco/File.h>
#include <Poco/Path.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

#include "config.h"
#include "database.h"
#include "recurrence_utils.h"

namespace {
DuckEvent eventAt(const int64_t id, const int64_t startMs, const int64_t eventStatId = 1) {
  DuckEvent event;
  event.id = id;
  event.start_date = startMs;
  event.event_stat_id = eventStatId;
  return event;
}
} // namespace

TEST(RecurrenceUtilsTest, EventsForClientCombinesMaterializedAndVirtualOccurrences) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_events_for_client_recurrence")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  DuckClient client;
  client.name = std::string{"Eve"};
  client.last_name = std::string{"E"};
  const auto clientId = db.add_client(client);
  ASSERT_GT(clientId, 0);

  const auto windowStart = QDateTime::currentDateTime().addMonths(-3);
  const auto windowEnd = QDateTime::currentDateTime().addMonths(3);

  DuckEvent standalone;
  standalone.name = std::string{"One-off session"};
  standalone.start_date = QDateTime::currentDateTime().addDays(1).toUTC().toMSecsSinceEpoch();
  standalone.end_date = *standalone.start_date + 3'600'000;
  const auto standaloneId = db.add_event(standalone);
  ASSERT_GT(standaloneId, 0);
  ASSERT_GT(db.add_event_client(standaloneId, clientId), 0);

  DuckEventSeries series;
  series.name = std::string{"Weekly with Eve"};
  series.client_id = clientId;
  series.start_date = QDateTime::currentDateTime().addDays(2).toUTC().toMSecsSinceEpoch();
  series.end_date = *series.start_date + 3'600'000;
  series.duration = 3600;
  series.recurrence_rule =
      pcm::recurrence::weeklyRuleForDate(QDateTime::currentDateTime().addDays(2).date()).toStdString();
  const auto seriesId = db.add_event_series(series);
  ASSERT_GT(seriesId, 0);

  const auto events =
      pcm::recurrence::eventsForClient(db, clientId, windowStart, windowEnd);

  const auto standaloneCount =
      std::count_if(events.begin(), events.end(),
                    [standaloneId](const DuckEvent &e) { return e.id == standaloneId; });
  EXPECT_EQ(standaloneCount, 1);

  const auto virtualCount =
      std::count_if(events.begin(), events.end(),
                    [seriesId](const DuckEvent &e) {
                      return e.is_virtual_occurrence && e.series_id == seriesId;
                    });
  EXPECT_GE(virtualCount, 1);

  db_dir.remove(true);
}

TEST(RecurrenceUtilsTest, EventsForClientSkipsMaterializedOccurrenceOfSameSeries) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_events_for_client_materialized")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  DuckClient client;
  client.name = std::string{"Frank"};
  client.last_name = std::string{"F"};
  const auto clientId = db.add_client(client);
  ASSERT_GT(clientId, 0);

  const auto seriesStartLocal =
      QDateTime(QDate::currentDate().addDays(1), QTime(10, 0, 0));
  DuckEventSeries series;
  series.name = std::string{"Weekly with Frank"};
  series.client_id = clientId;
  series.start_date = seriesStartLocal.toUTC().toMSecsSinceEpoch();
  series.end_date = *series.start_date + 3'600'000;
  series.duration = 3600;
  series.recurrence_rule =
      pcm::recurrence::weeklyRuleForDate(seriesStartLocal.date()).toStdString();
  const auto seriesId = db.add_event_series(series);
  ASSERT_GT(seriesId, 0);

  DuckEvent materialized;
  materialized.name = std::string{"Weekly with Frank"};
  materialized.start_date = seriesStartLocal.toUTC().toMSecsSinceEpoch();
  materialized.end_date = *materialized.start_date + 3'600'000;
  materialized.series_id = seriesId;
  materialized.original_occurrence_start = materialized.start_date;
  const auto materializedId = db.add_event(materialized);
  ASSERT_GT(materializedId, 0);
  ASSERT_GT(db.add_event_client(materializedId, clientId), 0);

  const auto windowStart = QDateTime::currentDateTime().addMonths(-3);
  const auto windowEnd = QDateTime::currentDateTime().addMonths(3);
  const auto events =
      pcm::recurrence::eventsForClient(db, clientId, windowStart, windowEnd);

  const auto occurrencesAtSeriesStart =
      std::count_if(events.begin(), events.end(), [&](const DuckEvent &e) {
        return e.start_date.has_value() &&
               std::abs(*e.start_date - *materialized.start_date) < 1000;
      });
  EXPECT_EQ(occurrencesAtSeriesStart, 1);

  db_dir.remove(true);
}

TEST(RecurrenceUtilsTest, LastAndNextAppointmentPastOnlyLeavesNextEmpty) {
  const QVector<DuckEvent> events{eventAt(1, 1'000)};
  const auto result = pcm::recurrence::lastAndNextAppointment(events, 2'000);
  ASSERT_TRUE(result.last.has_value());
  EXPECT_EQ(result.last->id, 1);
  EXPECT_FALSE(result.next.has_value());
}

TEST(RecurrenceUtilsTest, LastAndNextAppointmentFutureOnlyLeavesLastEmpty) {
  const QVector<DuckEvent> events{eventAt(1, 3'000)};
  const auto result = pcm::recurrence::lastAndNextAppointment(events, 2'000);
  EXPECT_FALSE(result.last.has_value());
  ASSERT_TRUE(result.next.has_value());
  EXPECT_EQ(result.next->id, 1);
}

TEST(RecurrenceUtilsTest, LastAndNextAppointmentSkipsCanceledEvents) {
  const QVector<DuckEvent> events{
      eventAt(1, 2'500, /*event_stat_id=*/3), // canceled, closer to now
      eventAt(2, 3'000, /*event_stat_id=*/1), // scheduled, farther
  };
  const auto result = pcm::recurrence::lastAndNextAppointment(events, 2'000);
  ASSERT_TRUE(result.next.has_value());
  EXPECT_EQ(result.next->id, 2);
}
