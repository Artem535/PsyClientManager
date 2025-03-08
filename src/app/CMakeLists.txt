cmake_minimum_required(VERSION 3.28)

# Set the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME ${PROJECT_NAME}_app)


# Add the library for the app module
add_library(${TARGET_NAME} STATIC)

# Add app.cppm as a C++20 module
target_sources(${TARGET_NAME}
PUBLIC
FILE_SET app_module
TYPE CXX_MODULES
FILES app.cppm
)

add_dependencies(${TARGET_NAME} 
    ${PROJECT_NAME}_config
    ${PROJECT_NAME}_widgets 
    ${PROJECT_NAME}_app_database 
    ${PROJECT_NAME}_logic
)

# Link the Slint UI files to this module
slint_target_sources(${TARGET_NAME}
  ${CMAKE_SOURCE_DIR}/ui/app/mainwindow.slint
  LIBRARY_PATHS widgets=${CMAKE_SOURCE_DIR}/ui/widgets
                pages=${CMAKE_SOURCE_DIR}/ui/pages
                logic=${CMAKE_SOURCE_DIR}/ui/logic
)


# Link Slint to the module
target_link_libraries(${TARGET_NAME} PUBLIC
    Slint::Slint
    ${PROJECT_NAME}_config
    ${PROJECT_NAME}_widgets 
    ${PROJECT_NAME}_app_database
    ${PROJECT_NAME}_logic
)
