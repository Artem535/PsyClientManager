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

# Link the Slint UI files to this module
slint_target_sources(${TARGET_NAME}
  ${CMAKE_SOURCE_DIR}/ui/app/mainwindow.slint
  LIBRARY_PATHS widgets=${CMAKE_SOURCE_DIR}/ui/widgets
)

# Link Slint to the module
target_link_libraries(${TARGET_NAME} PUBLIC Slint::Slint PsyClientManager_logic PsyClientManager_widgets)
