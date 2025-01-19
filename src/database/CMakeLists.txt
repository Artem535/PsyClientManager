set(TARGET_NAME ${PROJECT_NAME}_app_db)

include(FetchContent)
FetchContent_Declare(
    objectbox
    GIT_REPOSITORY https://github.com/objectbox/objectbox-c.git
    GIT_TAG        v4.0.3
)

FetchContent_MakeAvailable(objectbox)
find_package(ObjectBoxGenerator 4.0.0 REQUIRED)

add_library(${TARGET_NAME} STATIC scheme.obx.cpp)

target_sources(${TARGET_NAME}
    PUBLIC
    FILE_SET database_module
    TYPE CXX_MODULES
    FILES database.cppm
)

target_sources(${TARGET_NAME}
    PUBLIC
    FILE_SET headers
    TYPE HEADERS
    FILES objectbox-model.h
          scheme.obx.hpp
)

add_obx_schema(
    TARGET ${TARGET_NAME}
    SCHEMA_FILES scheme.fbs
    INSOURCE # Opt-in: Generate in source directory
)

target_link_libraries(${TARGET_NAME} PUBLIC objectbox)
