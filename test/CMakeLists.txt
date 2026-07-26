cmake_minimum_required(VERSION 3.28)
project(PCM_Tests VERSION 0.1 LANGUAGES CXX)

enable_testing()
find_package(GTest REQUIRED)
include(GoogleTest)

# Find Poco
find_package(Poco REQUIRED COMPONENTS Foundation Zip)

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

# Schedule conflict tests
add_executable(PsyClientManager_schedule_conflict_tests
    schedule_conflict_service_tests.cpp
)
target_link_libraries(PsyClientManager_schedule_conflict_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    PsyClientManager_event_view
)
gtest_discover_tests(PsyClientManager_schedule_conflict_tests)

# Backup tests
find_package(reflectcpp CONFIG REQUIRED)
add_executable(PsyClientManager_backup_tests backup_tests.cpp)
target_link_libraries(PsyClientManager_backup_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    PsyClientManager_config
    PsyClientManager_database
    PsyClientManager_backup
    Poco::Foundation
    Poco::Zip
    reflectcpp::reflectcpp
)
gtest_discover_tests(PsyClientManager_backup_tests)
