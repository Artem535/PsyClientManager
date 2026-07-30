#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/Timestamp.h>
#include <duckdb.hpp>
#include <gtest/gtest.h>
#include <limits>
#include "config.h"
#include "database.h"

TEST(DatabaseTest, InitDatabase) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  EXPECT_NO_THROW(pcm::database::Database{conf});

  db_dir.remove(true);
}

TEST(DatabaseTest, PersistsApplicationMetadataAcrossRestart) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_metadata")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  DuckApplicationMetadata first;
  {
    pcm::database::Database db{conf};
    first = db.get_application_metadata();
  }

  ASSERT_EQ(first.schema_version, 1);
  ASSERT_EQ(first.backup_format_version, 1);
  ASSERT_FALSE(first.workspace_uuid.empty());
  ASSERT_GT(first.created_at, 0);
  ASSERT_GT(first.last_migration_at, 0);

  {
    pcm::database::Database db{conf};
    const auto second = db.get_application_metadata();
    EXPECT_EQ(second.workspace_uuid, first.workspace_uuid);
    EXPECT_EQ(second.created_at, first.created_at);
    EXPECT_GE(second.last_migration_at, first.last_migration_at);
  }

  db_dir.remove(true);
}

TEST(DatabaseTest, ExportSnapshotWritesConsistentParquetFiles) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_export")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  DuckClient client;
  client.name = std::string{"Export"};
  client.last_name = std::string{"Test"};
  ASSERT_GT(db.add_client(client), 0);

  const auto exportDir =
      Poco::Path(Poco::Path::current()).append("tmp_dir_export_snapshot").toString();
  Poco::File exportDirFile(exportDir);
  if (exportDirFile.exists()) {
    exportDirFile.remove(true);
  }

  ASSERT_TRUE(db.export_snapshot(exportDir));

  EXPECT_TRUE(Poco::File(Poco::Path(exportDir).append("schema.sql")).exists());
  EXPECT_TRUE(Poco::File(Poco::Path(exportDir).append("load.sql")).exists());
  EXPECT_TRUE(Poco::File(Poco::Path(exportDir).append("client.parquet")).exists());

  Poco::File(exportDir).remove(true);
  db_dir.remove(true);
}

TEST(DatabaseTest, AddClientAndEvent) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  DuckClient client;
  client.name = std::string{"Test"};
  client.last_name = std::string{"User"};
  const auto clientId = db.add_client(client);
  EXPECT_GT(clientId, 0);

  DuckEvent event;
  event.name = std::string{"Test Event"};
  event.start_date = 1730000000000; // ms since epoch
  event.end_date = 1730003600000;   // +1 hour
  event.duration = 3600;
  event.cost = 3200.0;
  event.payment_stat_id = 2;
  event.event_stat_id = 3;
  event.cancellation_reason = std::string{"Client request"};
  event.canceled_by = std::string{"client"};

  const auto eventId = db.add_event(event);
  EXPECT_GT(eventId, 0);

  const auto storedEvent = db.get_event(eventId);
  ASSERT_NE(storedEvent, nullptr);
  EXPECT_EQ(storedEvent->id, eventId);
  EXPECT_EQ(storedEvent->name.value_or(""), "Test Event");
  ASSERT_TRUE(storedEvent->cost.has_value());
  EXPECT_DOUBLE_EQ(*storedEvent->cost, 3200.0);
  EXPECT_EQ(storedEvent->payment_stat_id, 2);
  EXPECT_EQ(storedEvent->event_stat_id, 3);
  ASSERT_TRUE(storedEvent->cancellation_reason.has_value());
  EXPECT_EQ(*storedEvent->cancellation_reason, "Client request");
  ASSERT_TRUE(storedEvent->canceled_by.has_value());
  EXPECT_EQ(*storedEvent->canceled_by, "client");

  const auto eventClientId = db.add_event_client(eventId, clientId);
  EXPECT_GT(eventClientId, 0);

  db_dir.remove(true);
}

TEST(DatabaseTest, PersistsConfirmedAtOnEvent) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_confirmed_at")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  DuckEvent event;
  event.name = std::string{"Session"};
  event.start_date = 1730000000000;
  event.end_date = 1730003600000;
  const auto eventId = db.add_event(event);
  ASSERT_GT(eventId, 0);

  const auto unconfirmed = db.get_event(eventId);
  ASSERT_NE(unconfirmed, nullptr);
  EXPECT_FALSE(unconfirmed->confirmed_at.has_value());

  auto toConfirm = *unconfirmed;
  toConfirm.confirmed_at = 1730000500000;
  ASSERT_TRUE(db.update_event(toConfirm));

  const auto confirmed = db.get_event(eventId);
  ASSERT_NE(confirmed, nullptr);
  ASSERT_TRUE(confirmed->confirmed_at.has_value());
  EXPECT_EQ(*confirmed->confirmed_at, 1730000500000);

  auto toUnconfirm = *confirmed;
  toUnconfirm.confirmed_at = std::nullopt;
  ASSERT_TRUE(db.update_event(toUnconfirm));

  const auto unconfirmedAgain = db.get_event(eventId);
  ASSERT_NE(unconfirmedAgain, nullptr);
  EXPECT_FALSE(unconfirmedAgain->confirmed_at.has_value());

  db_dir.remove(true);
}

TEST(DatabaseTest, DashboardIncomeCountsOnlyPaidEvents) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_income")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};

  const auto nowMs =
      static_cast<int64_t>(Poco::Timestamp{}.epochMicroseconds() / 1000);

  DuckEvent paidEvent;
  paidEvent.name = std::string{"Paid Event"};
  paidEvent.is_work_event = true;
  paidEvent.start_date = nowMs;
  paidEvent.end_date = nowMs + 3600000;
  paidEvent.duration = 3600;
  paidEvent.cost = 5000.0;
  paidEvent.payment_stat_id = 2;
  EXPECT_GT(db.add_event(paidEvent), 0);

  DuckEvent pendingEvent;
  pendingEvent.name = std::string{"Pending Event"};
  pendingEvent.is_work_event = true;
  pendingEvent.start_date = nowMs + 7200000;
  pendingEvent.end_date = nowMs + 10800000;
  pendingEvent.duration = 3600;
  pendingEvent.cost = 7000.0;
  pendingEvent.payment_stat_id = 1;
  EXPECT_GT(db.add_event(pendingEvent), 0);

  const auto summary = db.get_dashboard_summary();
  EXPECT_DOUBLE_EQ(summary.income_this_month, 5000.0);

  db_dir.remove(true);
}

TEST(DatabaseTest, DashboardExcludesCanceledNoShowAndRescheduledEvents) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_statuses")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};
  const auto nowMs =
      static_cast<int64_t>(Poco::Timestamp{}.epochMicroseconds() / 1000);
  const auto before = db.get_dashboard_summary();

  DuckEvent scheduled;
  scheduled.name = std::string{"Scheduled"};
  scheduled.is_work_event = true;
  scheduled.event_stat_id = 1;
  scheduled.payment_stat_id = 2;
  scheduled.start_date = nowMs;
  scheduled.end_date = nowMs + 3600000;
  scheduled.duration = 3600;
  scheduled.cost = 5000.0;
  EXPECT_GT(db.add_event(scheduled), 0);

  for (const auto statusId : {3LL, 5LL, 6LL}) {
    DuckEvent excluded = scheduled;
    excluded.id = -1;
    excluded.name = std::string{"Excluded"};
    excluded.event_stat_id = statusId;
    excluded.start_date = nowMs + 7200000 + statusId * 1000;
    excluded.end_date = *excluded.start_date + 3600000;
    excluded.cost = 7000.0;
    const auto excludedId = db.add_event(excluded);
    EXPECT_GT(excludedId, 0);
    const auto storedExcluded = db.get_event(excludedId);
    ASSERT_NE(storedExcluded, nullptr);
    EXPECT_EQ(storedExcluded->event_stat_id, statusId);
  }

  const auto after = db.get_dashboard_summary();
  EXPECT_EQ(after.sessions_this_month, before.sessions_this_month + 1);
  EXPECT_EQ(after.work_sessions_this_month, before.work_sessions_this_month + 1);
  EXPECT_DOUBLE_EQ(after.income_this_month, before.income_this_month + 5000.0);

  db_dir.remove(true);
}

TEST(DatabaseTest, PersistsBuffersAndRejectsBufferedAdjacentEvent) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_buffers")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};
  constexpr int64_t startMs = 4102444800000LL;

  DuckEvent event;
  event.name = std::string{"Buffered Event"};
  event.start_date = startMs;
  event.end_date = startMs + 3600000;
  event.duration = 3600;
  event.buffer_before_minutes = 10;
  event.buffer_after_minutes = 15;

  const auto eventId = db.add_event(event, false);
  ASSERT_GT(eventId, 0);
  const auto storedEvent = db.get_event(eventId);
  ASSERT_NE(storedEvent, nullptr);
  EXPECT_EQ(storedEvent->buffer_before_minutes, 10);
  EXPECT_EQ(storedEvent->buffer_after_minutes, 15);

  DuckEvent adjacent = event;
  adjacent.id = -1;
  adjacent.name = std::string{"Adjacent Event"};
  adjacent.start_date = startMs + 3600000 + 5 * 60000;
  adjacent.end_date = *adjacent.start_date + 3600000;
  adjacent.buffer_before_minutes = 0;
  adjacent.buffer_after_minutes = 0;
  EXPECT_EQ(db.add_event(adjacent, false), 0);

  db_dir.remove(true);
}

TEST(DatabaseTest, HandlesNullBufferColumnsFromLegacyDatabase) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_null_buffers")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};
  DuckEvent event;
  event.name = std::string{"Legacy Event"};
  event.start_date = 1730000000000;
  event.end_date = 1730003600000;
  const auto eventId = db.add_event(event);
  ASSERT_GT(eventId, 0);
  DuckEventSeries series;
  series.name = std::string{"Legacy Series"};
  series.start_date = 1730000000000;
  series.end_date = 1730003600000;
  series.duration = 3600;
  series.recurrence_rule = "FREQ=WEEKLY;INTERVAL=1";
  const auto seriesId = db.add_event_series(series);
  ASSERT_GT(seriesId, 0);
  duckdb::DuckDB rawDatabase((conf.db_conf().db_pth.toString() + "/database.db").c_str());
  duckdb::Connection rawConnection(rawDatabase);
  ASSERT_FALSE(rawConnection.Query(
      "UPDATE Event SET buffer_before_minutes = NULL, buffer_after_minutes = NULL")
                   ->HasError());
  ASSERT_FALSE(rawConnection.Query(
      "UPDATE EventSeries SET buffer_before_minutes = NULL, buffer_after_minutes = NULL")
                   ->HasError());
  auto nullCheck = rawConnection.Query(
      "SELECT buffer_before_minutes, buffer_after_minutes FROM Event WHERE id = " +
      std::to_string(eventId));
  ASSERT_FALSE(nullCheck->HasError());
  const auto nullChunk = nullCheck->Fetch();
  ASSERT_NE(nullChunk, nullptr);
  ASSERT_TRUE(nullChunk->GetValue(0, 0).IsNull());
  ASSERT_TRUE(nullChunk->GetValue(1, 0).IsNull());

  EXPECT_NO_THROW(db.get_event(eventId));
  EXPECT_NO_THROW(db.get_event_series(seriesId));
  EXPECT_NO_THROW(db.get_day_events(0, std::numeric_limits<int64_t>::max()));

  db_dir.remove(true);
}

TEST(DatabaseTest, TracksSeriesOccurrenceReminderNotifications) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_series_reminder")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};
  DuckEventSeries series;
  series.name = std::string{"Weekly Session"};
  series.start_date = 1730000000000;
  series.end_date = 1740000000000;
  series.duration = 3600;
  series.recurrence_rule = "FREQ=WEEKLY;INTERVAL=1";
  const auto seriesId = db.add_event_series(series);
  ASSERT_GT(seriesId, 0);

  const int64_t occurrenceStartMs = 1730000000000;
  const int64_t notifiedAtMs = 1730000000000;

  EXPECT_TRUE(
      db.get_notified_series_occurrences_for_range(0, occurrenceStartMs + 1)
          .empty());

  EXPECT_TRUE(db.mark_series_occurrence_reminder_notified(
      seriesId, occurrenceStartMs, notifiedAtMs));

  const auto notified =
      db.get_notified_series_occurrences_for_range(0, occurrenceStartMs + 1);
  ASSERT_EQ(notified.size(), 1u);
  EXPECT_TRUE(notified.contains({seriesId, occurrenceStartMs}));

  // Marking the same occurrence again must not create a duplicate row or error.
  EXPECT_TRUE(db.mark_series_occurrence_reminder_notified(
      seriesId, occurrenceStartMs, notifiedAtMs));
  EXPECT_EQ(
      db.get_notified_series_occurrences_for_range(0, occurrenceStartMs + 1).size(),
      1u);

  db_dir.remove(true);
}

TEST(DatabaseTest, FindsMaterializedOccurrenceRegardlessOfCurrentWindow) {
  pcm::config::Config conf{
      .db_conf = pcm::config::DatabaseConfig{
          .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir_materialized_override")}};

  auto db_dir = Poco::File(conf.db_conf().db_pth);
  if (db_dir.exists()) {
    db_dir.remove(true);
  }

  pcm::database::Database db{conf};
  DuckEventSeries series;
  series.name = std::string{"Weekly Session"};
  series.start_date = 1730000000000;
  series.end_date = 1740000000000;
  series.duration = 3600;
  series.recurrence_rule = "FREQ=WEEKLY;INTERVAL=1";
  const auto seriesId = db.add_event_series(series);
  ASSERT_GT(seriesId, 0);

  EXPECT_TRUE(
      db.get_materialized_occurrence_starts_for_series(seriesId).empty());

  // Reschedule a single occurrence far outside its original slot: the
  // override's original_occurrence_start stays pinned to the old slot, but
  // start_date/end_date move to a time weeks away.
  const int64_t originalOccurrenceStartMs = 1730000000000;
  DuckEvent overrideEvent;
  overrideEvent.name = std::string{"Weekly Session"};
  overrideEvent.series_id = seriesId;
  overrideEvent.original_occurrence_start = originalOccurrenceStartMs;
  overrideEvent.start_date = 1735000000000;
  overrideEvent.end_date = 1735003600000;
  const auto overrideId = db.add_event(overrideEvent);
  ASSERT_GT(overrideId, 0);

  const auto materializedStarts =
      db.get_materialized_occurrence_starts_for_series(seriesId);
  ASSERT_EQ(materializedStarts.size(), 1u);
  EXPECT_TRUE(materializedStarts.contains(originalOccurrenceStartMs));

  db_dir.remove(true);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
