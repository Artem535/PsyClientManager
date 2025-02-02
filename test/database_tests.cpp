#include <Poco/File.h>
#include <gtest/gtest.h>
#include <Poco/Path.h>

import config;
import app_database;

TEST(DatabaseTest, InitDatabase) {
  pcm::config::Config conf{
    .db_conf = pcm::config::DatabaseConfig{
      .db_pth = Poco::Path(Poco::Path::current()).append("tmp_dir")
    }
  };

  EXPECT_NO_THROW(pcm::database::Database{conf});
  Poco::File(conf.db_conf().db_pth).remove(true);
}


int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  
  return RUN_ALL_TESTS();
}
