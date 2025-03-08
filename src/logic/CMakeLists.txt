set(TARGET_NAME ${PROJECT_NAME}_logic)

# Add the library for the app module
add_library(${TARGET_NAME} STATIC)

# Add logic.cppm as a C++20 module
target_sources(${TARGET_NAME}
  PUBLIC
  FILE_SET logic_modules
  TYPE CXX_MODULES
  FILES
  logic.cppm
)

add_dependencies(${TARGET_NAME}
  ${PROJECT_NAME}_widgets
  ${PROJECT_NAME}_app_database
)

# Link Slint to the module
target_link_libraries(${TARGET_NAME}
  PUBLIC
  Slint::Slint
  ${PROJECT_NAME}_widgets
  ${PROJECT_NAME}_app_database)

slint_target_sources(${TARGET_NAME}
  ${CMAKE_SOURCE_DIR}/ui/logic/logic.slint
  LIBRARY_PATHS widgets=${CMAKE_SOURCE_DIR}/ui/widgets
  pages=${CMAKE_SOURCE_DIR}/ui/pages
)
