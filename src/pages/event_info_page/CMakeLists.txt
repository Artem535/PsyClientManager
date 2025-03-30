cmake_minimum_required(VERSION 3.28)

# Set the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME ${PROJECT_NAME}_event_page)
set(KDChart-qt6_DIR /usr/local/KDAB/KDChart-3.1.0/lib64/cmake/KDChart-qt6/)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets PrintSupport Gui)
find_package(KDChart-qt6 CONFIG REQUIRED)

# Add the library for the app module
qt_add_library(${TARGET_NAME} STATIC
    eventinfo.cpp
)

qt_add_ui(${TARGET_NAME} SOURCES
    ../../../ui/pages/eventinfo.ui
    INCLUDE_PREFIX ui/pages
)

add_dependencies(${TARGET_NAME}
    ${PROJECT_NAME}_config
    ${PROJECT_NAME}_database
    ${PROJECT_NAME}_client_model
    ${PROJECT_NAME}_event_model_view
)

# Link Slint to the module
target_link_libraries(${TARGET_NAME} PUBLIC
    ${PROJECT_NAME}_config
    ${PROJECT_NAME}_database
    ${PROJECT_NAME}_client_model
    ${PROJECT_NAME}_event_model_view
    Qt6::Widgets
    Qt6::Gui

    KDChart::kdchart
)

target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

