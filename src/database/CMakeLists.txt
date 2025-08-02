set(TARGET_NAME ${PROJECT_NAME}_database)

include(FetchContent)
FetchContent_Declare(
    objectbox
    GIT_REPOSITORY https://github.com/objectbox/objectbox-c.git
    GIT_TAG v4.0.3
)

FetchContent_MakeAvailable(objectbox)

find_package(ObjectBoxGenerator 4.0.0 REQUIRED)
find_package(Poco REQUIRED COMPONENTS Foundation)

add_library(${TARGET_NAME} STATIC 
    database.cpp
    constance.hpp
    scheme.obx.cpp
    datetime.hpp
)

# Include directories
target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_dependencies(${TARGET_NAME} ${PROJECT_NAME}_config objectbox)

add_obx_schema(
    TARGET ${TARGET_NAME}
    SCHEMA_FILES scheme.fbs
    INSOURCE # Opt-in: Generate in source directory
)

target_link_libraries(${TARGET_NAME} objectbox ${PROJECT_NAME}_config Poco::Foundation)