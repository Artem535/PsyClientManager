cmake_minimum_required(VERSION 3.28)

# Set the C++ standard
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Define the project
project(PsyClientManager VERSION 0.1.0 LANGUAGES CXX)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Svg PrintSupport LinguistTools)
include(GNUInstallDirs)
option(PCM_BUILD_TESTS "Build project tests" OFF)

if(Qt6_VERSION VERSION_LESS 6.8.0)
  message(FATAL_ERROR "Qlementine requires Qt 6.8.0 or newer. Detected Qt ${Qt6_VERSION}.")
endif()

set(QLEMENTINE_PATCH_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/apply_qlementine_patch.cmake")
execute_process(
  COMMAND "${CMAKE_COMMAND}" -P "${QLEMENTINE_PATCH_SCRIPT}"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  RESULT_VARIABLE QLEMENTINE_PATCH_RESULT
  OUTPUT_VARIABLE QLEMENTINE_PATCH_OUTPUT
  ERROR_VARIABLE QLEMENTINE_PATCH_ERROR
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_STRIP_TRAILING_WHITESPACE
)

if(NOT QLEMENTINE_PATCH_RESULT EQUAL 0)
  message(FATAL_ERROR
    "Failed to apply local Qlementine patch.\n"
    "stdout: ${QLEMENTINE_PATCH_OUTPUT}\n"
    "stderr: ${QLEMENTINE_PATCH_ERROR}")
endif()

if(QLEMENTINE_PATCH_OUTPUT)
  message(STATUS "${QLEMENTINE_PATCH_OUTPUT}")
endif()

add_custom_target(apply_qlementine_patch
  COMMAND "${CMAKE_COMMAND}" -P "${QLEMENTINE_PATCH_SCRIPT}"
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  COMMENT "Applying local patch to third_party/qlementine"
  VERBATIM
)

set(QLEMENTINE_SANDBOX OFF CACHE BOOL "" FORCE)
set(QLEMENTINE_SHOWCASE OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/qlementine)

qt_add_library(qcustomplot STATIC
    third_party/qcustomplot/qcustomplot.cpp
    third_party/qcustomplot/qcustomplot.h
)
target_include_directories(qcustomplot PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/qcustomplot
)
set_target_properties(qcustomplot PROPERTIES
    AUTOMOC ON
    AUTOUIC OFF
    AUTORCC OFF
)
target_link_libraries(qcustomplot PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::PrintSupport
)

qt_standard_project_setup()

qt_add_translations(PsyClientManager
    TS_FILES
        translation/app_ru.ts
        translation/app_en.ts
)

# Include subdirectories for modules
add_subdirectory(src/widgets)
add_subdirectory(src/config)
add_subdirectory(src/database)
add_subdirectory(src/client_model)
add_subdirectory(src/pages/client_info_page)
add_subdirectory(src/pages/event_info_page)
add_subdirectory(src/pages/detail_client_info_page)
add_subdirectory(src/pages/analytics_page)
add_subdirectory(src/timeline_widget)
add_subdirectory(src/event_view)
add_subdirectory(src/app)

include_directories(src/app)

if(PCM_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()

# Add the main executable
qt_add_executable(PsyClientManager src/main.cpp
        resources/themes.qrc)

add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}_app)

# Link with libs
target_link_libraries(PsyClientManager PUBLIC ${PROJECT_NAME}_app Qt6::Widgets)

install(TARGETS PsyClientManager
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES
    ${CMAKE_SOURCE_DIR}/LICENSE
    ${CMAKE_SOURCE_DIR}/README.md
    DESTINATION .
)

if(UNIX AND NOT APPLE)
  install(FILES
      ${CMAKE_SOURCE_DIR}/packaging/PsyClientManager.desktop
      DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
  )
  install(FILES
      ${CMAKE_SOURCE_DIR}/resources/icons/brain-solid-full.svg
      DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
      RENAME PsyClientManager.svg
  )
endif()

qt_generate_deploy_app_script(
    TARGET PsyClientManager
    OUTPUT_SCRIPT deploy_runtime_dependencies_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_runtime_dependencies_script})

set(CPACK_PACKAGE_NAME "PsyClientManager")
set(CPACK_PACKAGE_VENDOR "PsyClientManager")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}"
)
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")

if(WIN32)
  set(CPACK_GENERATOR "ZIP")
endif()

include(CPack)
