#include <Poco/File.h>
#include <Poco/Path.h>
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

  const auto eventId = db.add_event(event);
  EXPECT_GT(eventId, 0);

  const auto storedEvent = db.get_event(eventId);
  ASSERT_NE(storedEvent, nullptr);
  EXPECT_EQ(storedEvent->id, eventId);
  EXPECT_EQ(storedEvent->name.value_or(""), "Test Event");

  const auto eventClientId = db.add_event_client(eventId, clientId);
  EXPECT_GT(eventClientId, 0);

  db_dir.remove(true);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
