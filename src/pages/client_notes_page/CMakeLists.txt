cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME ${PROJECT_NAME}_client_notes_page)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

qt_add_library(${TARGET_NAME} STATIC
        client_notes_page.cpp
        client_notes_page.h
)

add_dependencies(${TARGET_NAME}
        ${PROJECT_NAME}_database
)

target_link_libraries(${TARGET_NAME} PUBLIC
        ${PROJECT_NAME}_database
        Qt6::Widgets
)

target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
