#include <Poco/File.h>
#include <Poco/Path.h>
#include <gtest/gtest.h>

import config;
import app_database;

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

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
