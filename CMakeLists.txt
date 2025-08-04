cmake_minimum_required(VERSION 3.28)

# Set the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Define the project
project(PsyClientManager VERSION 0.1.0 LANGUAGES CXX)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

qt_standard_project_setup()

# Include subdirectories for modules
add_subdirectory(src/config)
add_subdirectory(src/database)
add_subdirectory(src/client_model)
add_subdirectory(src/pages/client_info_page)
add_subdirectory(src/pages/event_info_page)
add_subdirectory(src/timeline_widget)
add_subdirectory(src/event_view)
add_subdirectory(src/event_data_manager)
add_subdirectory(src/app)

include_directories(src/app)

# Enabling tests 
enable_testing()
add_subdirectory(test)

# Add the main executable
qt_add_executable(PsyClientManager src/main.cpp)

add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_app)

# Link with libs
target_link_libraries(PsyClientManager PUBLIC ${PROJECT_NAME}_app Qt6::Widgets)