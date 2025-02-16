cmake_minimum_required(VERSION 3.28)
project(PCM_Tests VERSION 0.1 LANGUAGES CXX)

enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

# Find Poco
find_package(Poco REQUIRED COMPONENTS Foundation)

# Config tests
add_executable(PsyClientManager_config_tests config_tests.cpp)
target_link_libraries(PsyClientManager_config_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    PsyClientManager_config
)
gtest_discover_tests(PsyClientManager_config_tests)

# Database tests
add_executable(PsyClientManager_database_tests database_tests.cpp)
target_link_libraries(PsyClientManager_database_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    PsyClientManager_config
    PsyClientManager_database
    Poco::Foundation
)
gtest_discover_tests(PsyClientManager_database_tests)