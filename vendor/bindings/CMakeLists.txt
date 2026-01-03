cmake_minimum_required(VERSION 3.21)
project(saucer-bindings LANGUAGES CXX VERSION 5.0.0)

# --------------------------------------------------------------------------------------------------------
# Library switches
# --------------------------------------------------------------------------------------------------------

option(saucer_desktop         "Enable support for the desktop module" ON)
option(saucer_pdf             "Enable support for the pdf module"     ON)
option(saucer_bindings_static "Build static library for single-file distribution" OFF)

# --------------------------------------------------------------------------------------------------------
# CMake options
# --------------------------------------------------------------------------------------------------------

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --------------------------------------------------------------------------------------------------------
# Setup library
# --------------------------------------------------------------------------------------------------------

if (saucer_bindings_static)
  add_library(${PROJECT_NAME} STATIC)
  target_compile_definitions(${PROJECT_NAME} PUBLIC SAUCER_BINDINGS_STATIC_DEFINE)
else()
  add_library(${PROJECT_NAME} SHARED)
endif()
add_library(saucer::bindings ALIAS ${PROJECT_NAME})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23 CXX_EXTENSIONS OFF CXX_STANDARD_REQUIRED ON)

if (PROJECT_IS_TOP_LEVEL AND NOT MSVC AND NOT CMAKE_CXX_SIMULATE_ID MATCHES "MSVC")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror -pedantic -pedantic-errors -Wfatal-errors)
endif()

if (NOT MSVC)
  target_compile_options(${PROJECT_NAME} PRIVATE -Wno-unknown-warning-option -Wno-missing-field-initializers -Wno-cast-function-type)
endif()

# --------------------------------------------------------------------------------------------------------
# Export header
# --------------------------------------------------------------------------------------------------------

include("cmake/hide.cmake")
include("cmake/export.cmake")

saucer_bindings_hide_symbols(${PROJECT_NAME})
saucer_bindings_export(${PROJECT_NAME} "SAUCER_EXPORT")

# --------------------------------------------------------------------------------------------------------
# Include directories
# --------------------------------------------------------------------------------------------------------

target_include_directories(${PROJECT_NAME} PUBLIC  "include")
target_include_directories(${PROJECT_NAME} PRIVATE "include/saucer" "private")

# --------------------------------------------------------------------------------------------------------
# Add Sources
# --------------------------------------------------------------------------------------------------------

target_sources(${PROJECT_NAME} PRIVATE
    "src/app.cpp"
    "src/options.cpp"

    "src/memory.cpp"

    "src/icon.cpp"
    "src/stash.cpp"
    "src/script.cpp"
    "src/scheme.cpp"

    "src/navigation.cpp"
    "src/preferences.cpp"

    "src/window.cpp"
    "src/webview.cpp"
)

# --------------------------------------------------------------------------------------------------------
# Setup Dependencies
# --------------------------------------------------------------------------------------------------------

include("cmake/cpm.cmake")

set(_local_saucer "${CMAKE_CURRENT_SOURCE_DIR}/../../../saucer")
if(EXISTS "${_local_saucer}/CMakeLists.txt")
  message(STATUS "[saucer-bindings] Using local saucer at ${_local_saucer}")
  if (saucer_bindings_static)
    set(saucer_static ON CACHE BOOL "Build static saucer for single-file distribution" FORCE)
  else()
    set(saucer_static OFF CACHE BOOL "Build shared saucer for node bindings" FORCE)
  endif()
  set(saucer_prefer_remote OFF CACHE BOOL "Prefer remote saucer packages" FORCE)
  add_subdirectory("${_local_saucer}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/saucer-src")
else()
  CPMFindPackage(
    NAME           saucer
    VERSION        6.0.0
    GIT_REPOSITORY "https://github.com/saucer/saucer"
    OPTIONS        "saucer_static ${saucer_bindings_static}" "CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON"
  )
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC saucer::saucer)

# --------------------------------------------------------------------------------------------------------
# Setup Modules
# --------------------------------------------------------------------------------------------------------

include("cmake/module.cmake")

if (saucer_desktop)
  add_subdirectory("modules/desktop")
endif()

if (saucer_pdf)
  add_subdirectory("modules/pdf")
endif()
