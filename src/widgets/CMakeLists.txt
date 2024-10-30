set(TARGET_NAME ${PROJECT_NAME}_widgets)

# Add the library for the widgets module
add_library(${TARGET_NAME} STATIC)

# Add a custom command to generate the Slint UI header files
# add_custom_command(
#   OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/ui/widgets/calendar.h # Adjust path as necessary
#   COMMAND slint-compiler ${CMAKE_SOURCE_DIR}/ui/widgets/calendar.slint -o ${CMAKE_CURRENT_SOURCE_DIR}/ui/widgets/calendar.h
#   DEPENDS ${CMAKE_SOURCE_DIR}/ui/widgets/calendar.slint
#   COMMENT "Generating calendar.h from calendar.slint"
# )

# # Ensure that the target depends on the generated header
# add_custom_target(widgets_ui_generated DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/ui/widgets/calendar.h)

target_sources(${TARGET_NAME}
  PUBLIC
  FILE_SET widgets_modules
  TYPE CXX_MODULES
  FILES calendar.cppm
)

# add_dependencies(${TARGET_NAME} widgets_ui_generated)

# Link the Slint UI files to this module
slint_target_sources(${TARGET_NAME}
  ${CMAKE_SOURCE_DIR}/ui/widgets/calendar.slint
)

target_link_libraries(${TARGET_NAME} PUBLIC Slint::Slint)

# Link this module to the main executable
target_link_libraries(${PROJECT_NAME} PUBLIC ${TARGET_NAME})

