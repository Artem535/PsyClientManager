#include <gtest/gtest.h>

import config;
import app_database;


int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  
  return RUN_ALL_TESTS();
}
