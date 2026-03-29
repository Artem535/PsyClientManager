#include <Poco/File.h>
#include <Poco/Path.h>
#include <Poco/Timestamp.h>
#include <gtest/gtest.h>
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

  const auto eventId = db.add_event(event);
  EXPECT_GT(eventId, 0);

  const auto storedEvent = db.get_event(eventId);
  ASSERT_NE(storedEvent, nullptr);
  EXPECT_EQ(storedEvent->id, eventId);
  EXPECT_EQ(storedEvent->name.value_or(""), "Test Event");
  ASSERT_TRUE(storedEvent->cost.has_value());
  EXPECT_DOUBLE_EQ(*storedEvent->cost, 3200.0);
  EXPECT_EQ(storedEvent->payment_stat_id, 2);

  const auto eventClientId = db.add_event_client(eventId, clientId);
  EXPECT_GT(eventClientId, 0);

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

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
