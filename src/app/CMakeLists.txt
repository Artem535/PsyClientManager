cmake_minimum_required(VERSION 3.28)

# Set the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME ${PROJECT_NAME}_app)


# Add the library for the app module
qt_add_library(${TARGET_NAME} STATIC app.cpp)


add_dependencies(${TARGET_NAME} 
    ${PROJECT_NAME}_config
    ${PROJECT_NAME}_app_database 
)

# Link Slint to the module
target_link_libraries(${TARGET_NAME} PUBLIC
    ${PROJECT_NAME}_config
    ${PROJECT_NAME}_app_database
)
