#include <gtest/gtest.h>

import config;

TEST(ConfigTest, Initialize) {
    EXPECT_NO_THROW(pcm::config::Config());
}


int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  
  return RUN_ALL_TESTS();
}
