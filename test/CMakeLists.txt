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

# Recurrence utils tests
add_executable(PsyClientManager_recurrence_tests recurrence_utils_tests.cpp)
target_link_libraries(PsyClientManager_recurrence_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    PsyClientManager_config
    PsyClientManager_database
    PsyClientManager_event_view
    Poco::Foundation
)
gtest_discover_tests(PsyClientManager_recurrence_tests)

# Change-log line text tests
add_executable(PsyClientManager_change_log_text_tests change_log_text_tests.cpp)
target_link_libraries(PsyClientManager_change_log_text_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    PsyClientManager_config
    PsyClientManager_database
    PsyClientManager_client_notes_page
    Poco::Foundation
)
gtest_discover_tests(PsyClientManager_change_log_text_tests)

# Backup tests
find_package(reflectcpp CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
add_executable(PsyClientManager_backup_tests
    backup_tests.cpp
    ../src/app/auto_backup_scheduler.cpp
    ../src/app/backup_encryption_policy.cpp
    ../src/app/credential_store.cpp
    ../src/app/app_lock_service.cpp
    ../src/app/app_lock_controller.cpp
)
target_link_libraries(PsyClientManager_backup_tests PRIVATE
    GTest::gtest
    GTest::gtest_main
    PsyClientManager_config
    PsyClientManager_database
    PsyClientManager_backup
    PsyClientManager_widgets
    Poco::Foundation
    Poco::Zip
    reflectcpp::reflectcpp
    unofficial-sodium::sodium
    qt6keychain
)
target_include_directories(PsyClientManager_backup_tests PRIVATE
    ${qtkeychain_SOURCE_DIR}
    ${qtkeychain_BINARY_DIR}
)
gtest_discover_tests(PsyClientManager_backup_tests)
