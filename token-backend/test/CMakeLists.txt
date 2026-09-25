find_package(GTest REQUIRED)

add_executable(token_backend_tests
  health_controller_tests.cpp
)
target_link_libraries(token_backend_tests PRIVATE
  token_backend_lib
  GTest::gtest
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(token_backend_tests)
