cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)

project(PsyClientManager VERSION 0.1.0 LANGUAGES CXX)

find_package(Slint QUIET)
if (NOT Slint_FOUND)
  message("Slint could not be located in the CMake module search path. Downloading it from Git and building it locally")
  include(FetchContent)
  FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    GIT_TAG release/1
    SOURCE_SUBDIR api/cpp
  )
  FetchContent_MakeAvailable(Slint)
endif (NOT Slint_FOUND)

add_executable(PsyClientManager 
               src/main.cpp)

target_include_directories(PsyClientManager PUBLIC "${PROJECT_SOURCE_DIR}/include")

target_link_libraries(PsyClientManager PUBLIC Slint::Slint)

slint_target_sources(PsyClientManager ui/mainwindow.slint)
set_property(TARGET PsyClientManager PROPERTY SLINT_STYLE native)

