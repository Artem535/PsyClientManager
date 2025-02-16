cmake_minimum_required(VERSION 3.28)

# Set the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME ${PROJECT_NAME}_client_page)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Add the library for the app module
qt_add_library(${TARGET_NAME} STATIC
    clientinfo.cpp
    clientmodel.cpp
)


qt_add_ui(${TARGET_NAME} SOURCES
    ../../../ui/pages/clientinfo.ui
    INCLUDE_PREFIX ui/pages
)

add_dependencies(${TARGET_NAME}
    ${PROJECT_NAME}_config
    ${PROJECT_NAME}_database
)

# Link Slint to the module
target_link_libraries(${TARGET_NAME} PUBLIC
    ${PROJECT_NAME}_config
    ${PROJECT_NAME}_database
    Qt6::Widgets
)

target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

