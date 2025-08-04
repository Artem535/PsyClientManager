cmake_minimum_required(VERSION 3.28)

# Set the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME ${PROJECT_NAME}_event_view)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Add the library for the app module
qt_add_library(${TARGET_NAME} STATIC
    eventview.cpp
    eventitem.cpp
)

add_dependencies(${TARGET_NAME}
    ${PROJECT_NAME}_config
)

# Link Slint to the module
target_link_libraries(${TARGET_NAME} PUBLIC
    ${PROJECT_NAME}_config
    Qt6::Widgets
    Qt6::Gui
)

target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

