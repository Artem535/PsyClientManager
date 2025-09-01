cmake_minimum_required(VERSION 3.28)

# Set the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME ${PROJECT_NAME}_app)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)

# Add the library for the app module
qt_add_library(${TARGET_NAME} STATIC
        application.cpp
        main_window.cpp
)


qt_add_ui(${TARGET_NAME} SOURCES
        ../../ui/app/mainwindow.ui
        INCLUDE_PREFIX ui/app
        OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/generated_ui
)

add_dependencies(${TARGET_NAME}
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_client_page
        ${PROJECT_NAME}_client_info_card_page
        ${PROJECT_NAME}_event_page
)

# Link Slint to the module
target_link_libraries(${TARGET_NAME} PUBLIC
        ${PROJECT_NAME}_config
        ${PROJECT_NAME}_database
        ${PROJECT_NAME}_client_page
        ${PROJECT_NAME}_client_info_card_page
        ${PROJECT_NAME}_event_page
        Qt6::Widgets
)
