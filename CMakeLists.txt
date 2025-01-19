cmake_minimum_required(VERSION 3.28)

# Set the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)

# Define the project
project(PsyClientManager VERSION 0.1.0 LANGUAGES CXX)

# Find or fetch Slint
find_package(Slint QUIET)
if(NOT Slint_FOUND)
  message(STATUS "Slint could not be located in the CMake module search path. Downloading it from Git and building it locally.")
  include(FetchContent)
  FetchContent_Declare(
    Slint
    GIT_REPOSITORY https://github.com/slint-ui/slint.git
    GIT_TAG release/1
    SOURCE_SUBDIR api/cpp
  )
  FetchContent_MakeAvailable(Slint)
endif()



# Include subdirectories for modules
add_subdirectory(src/database)
add_subdirectory(src/widgets)
add_subdirectory(src/logic)
add_subdirectory(src/app)

# Add the main executable
add_executable(PsyClientManager src/main.cpp)

# Set Slint style
set_property(TARGET PsyClientManager PROPERTY SLINT_STYLE native)

# Link with modules
target_link_libraries(PsyClientManager PUBLIC PsyClientManager_app)
