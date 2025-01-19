set(TARGET_NAME ${PROJECT_NAME}_widgets)

# Add the library for the widgets module
add_library(${TARGET_NAME} STATIC)


target_sources(${TARGET_NAME}
  PUBLIC
  FILE_SET widgets_modules
  TYPE CXX_MODULES
  FILES calendar_widget_logic.cppm
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
)

# add_dependencies(${TARGET_NAME} widgets_ui_generated)

# Link the Slint UI files to this module
slint_target_sources(${TARGET_NAME}
  ${CMAKE_SOURCE_DIR}/ui/widgets/calendar.slint
)

target_link_libraries(${TARGET_NAME} PUBLIC Slint::Slint)
