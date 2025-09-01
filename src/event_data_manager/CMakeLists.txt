cmake_minimum_required(VERSION 3.28)

# Set the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME ${PROJECT_NAME}_event_data_manager)

find_package(Qt6 REQUIRED COMPONENTS Core)

# Add the library for the app module
qt_add_library(${TARGET_NAME} STATIC
        qevent_data_manager.cpp
)


add_dependencies(${TARGET_NAME}
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_event_view
)

# Link Slint to the module
target_link_libraries(${TARGET_NAME} PUBLIC
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_event_view
        Qt6::Core
)

target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

