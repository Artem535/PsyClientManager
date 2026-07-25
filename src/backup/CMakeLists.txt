set(TARGET_NAME ${PROJECT_NAME}_backup)

find_package(Poco REQUIRED COMPONENTS Foundation Zip)
find_package(reflectcpp CONFIG REQUIRED)

add_library(${TARGET_NAME} STATIC
        backup_manifest.hpp
        checksum_utils.hpp
        backup_service.h
        backup_service.cpp
)

target_include_directories(${TARGET_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_dependencies(${TARGET_NAME} ${PROJECT_NAME}_database)

target_link_libraries(${TARGET_NAME}
  PUBLIC
    ${PROJECT_NAME}_database
    Poco::Foundation
  PRIVATE
    Poco::Zip
    reflectcpp::reflectcpp
)
