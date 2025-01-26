cmake_minimum_required(VERSION 3.28)

# Set the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)

set(TARGET_NAME ${PROJECT_NAME}_config)

find_package(reflectcpp REQUIRED)
find_package(Poco REQUIRED)

# Add the library for the app module
add_library(${TARGET_NAME} STATIC)

# Add app.cppm as a C++20 module
target_sources(${TARGET_NAME}
PUBLIC
FILE_SET config_module
TYPE CXX_MODULES
FILES 
config.cppm
rfl_path.cppm
)

target_link_libraries(${TARGET_NAME} PRIVATE reflectcpp::reflectcpp Poco::Poco)