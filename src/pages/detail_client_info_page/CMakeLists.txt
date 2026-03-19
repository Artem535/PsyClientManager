cmake_minimum_required(VERSION 3.28)

# Set the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME ${PROJECT_NAME}_client_info_card_page)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets PrintSupport)

# Add the library for the app module
qt_add_library(${TARGET_NAME} STATIC
        client_info_card.cpp
        client_charts_widget.cpp
        qclient.cpp
        qclient.h
)

target_include_directories(${TARGET_NAME} PRIVATE
        ${qlementine_SOURCE_DIR}/lib/include
)


qt_add_ui(${TARGET_NAME} SOURCES
        ../../../ui/pages/detailclientinfo.ui
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
        qlementine
        qcustomplot
        Qt6::Widgets
        Qt6::PrintSupport
)

target_include_directories(${TARGET_NAME} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
