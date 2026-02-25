cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

project(saucer-nodejs LANGUAGES CXX)

# Enable Objective-C++ for macOS
if(APPLE)
  enable_language(OBJCXX)
  # Ensure packaged .node/.dylib artifacts can resolve sibling dylibs after
  # being copied out of the build tree (e.g. npm/<target>/ in CI artifacts).
  set(CMAKE_MACOSX_RPATH ON)
  set(CMAKE_BUILD_RPATH "@loader_path")
  set(CMAKE_INSTALL_RPATH "@loader_path")
  set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
endif()

# Force C++23 for all targets (required by saucer bindings)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_OBJCXX_STANDARD 23)
set(CMAKE_OBJCXX_STANDARD_REQUIRED ON)

# Ensure all dependencies use C++23
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23")
set(CMAKE_OBJCXX_FLAGS "${CMAKE_OBJCXX_FLAGS} -std=c++23")

# Add NAPI version
add_definitions(-DNAPI_VERSION=9)

# Get Node.js include directory from cmake-js or detect manually
if(CMAKE_JS_INC)
  message(STATUS "Using cmake-js include directories")
  set(NODE_INCLUDE_DIR ${CMAKE_JS_INC})
else()
  # Fallback: Get Node.js include directory manually
  if(WIN32)
    execute_process(
      COMMAND node -p "require('path').dirname(process.execPath) + '\\\\include\\\\node'"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      OUTPUT_VARIABLE NODE_INCLUDE_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  else()
    execute_process(
      COMMAND node -p "process.config.variables.node_prefix + '/include/node'"
      WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
      OUTPUT_VARIABLE NODE_INCLUDE_DIR
      OUTPUT_STRIP_TRAILING_WHITESPACE
    )
  endif()
endif()

message(STATUS "Node.js include directory: ${NODE_INCLUDE_DIR}")

# Locate libuv headers
# On Windows with cmake-js, uv.h should be in the cmake-js includes
# On macOS/Linux, it's typically bundled with Node or in system paths
if(WIN32)
  # On Windows, uv.h is in the node include directory
  find_path(UV_INCLUDE_DIR uv.h
    HINTS
      ${CMAKE_JS_INC}
      ${NODE_INCLUDE_DIR}
    NO_DEFAULT_PATH
  )
  # If not found, just use the node include dir (uv.h is bundled)
  if(NOT UV_INCLUDE_DIR)
    set(UV_INCLUDE_DIR ${NODE_INCLUDE_DIR})
  endif()
else()
  find_path(UV_INCLUDE_DIR uv.h
    HINTS
      ${CMAKE_JS_INC}
      ${NODE_INCLUDE_DIR}
      ${NODE_INCLUDE_DIR}/..
      /opt/homebrew/include
      /usr/local/include
      /usr/include
  )
endif()

if (NOT UV_INCLUDE_DIR AND NOT CMAKE_JS_INC)
  message(WARNING "Could not locate uv.h - build may fail if libuv is required")
else()
  message(STATUS "libuv include directory: ${UV_INCLUDE_DIR}")
endif()

# Include directories
include_directories(${CMAKE_JS_INC})
if(NODE_INCLUDE_DIR)
  include_directories(${NODE_INCLUDE_DIR})
endif()
if(UV_INCLUDE_DIR)
  include_directories(${UV_INCLUDE_DIR})
endif()
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/bindings/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/bindings/modules/desktop/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/bindings/modules/pdf/include")

# Source files
file(GLOB SOURCE_FILES "src/*.cpp")
file(GLOB COMPAT_SOURCE_FILES "src/compat/*.cpp")
list(APPEND SOURCE_FILES ${COMPAT_SOURCE_FILES})

# Add platform-specific files
if(APPLE)
  list(APPEND SOURCE_FILES "src/runloop_mac.mm")
  list(APPEND SOURCE_FILES "src/platform_mac.mm")
elseif(UNIX AND NOT APPLE)
  list(APPEND SOURCE_FILES "src/platform_linux.cpp")
elseif(WIN32)
  list(APPEND SOURCE_FILES "src/platform_win.cpp")
endif()

# MSVC specific configuration
if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
  execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
endif()

# Enable static linking for single-file distribution (prebuilt binaries)
set(saucer_bindings_static ON CACHE BOOL "Build static bindings for single .node file" FORCE)
set(saucer_static ON CACHE BOOL "Build static saucer library" FORCE)

# MSVC static runtime for Windows
if (MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# Enable saucer modules (pdf, desktop)
set(saucer_desktop ON CACHE BOOL "Enable desktop module" FORCE)
set(saucer_pdf ON CACHE BOOL "Enable PDF module" FORCE)
set(saucer_loop ON CACHE BOOL "Enable loop module" FORCE)

# Compatibility shim: saucer v8 uses saucer_no_compiler_version_check.
# Keep accepting the older saucer_no_version_check switch used by older scripts.
if(saucer_no_version_check)
  set(saucer_no_compiler_version_check ON CACHE BOOL "Disable saucer compiler version check" FORCE)
endif()

set(saucer_prefer_remote OFF CACHE BOOL "Prefer remote saucer packages" FORCE)
set(saucer_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../saucer" CACHE STRING "Use local saucer sources" FORCE)
set(CPM_USE_LOCAL_PACKAGES ON CACHE BOOL "Use local CPM packages when available" FORCE)
if(APPLE)
  set(saucer_backend WebKit CACHE STRING "Use WebKit backend on macOS" FORCE)
endif()

# Add vendored saucer bindings as a subdirectory
# The bindings will automatically fetch the saucer library from GitHub via CPM
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/vendor/bindings" "${CMAKE_CURRENT_BINARY_DIR}/bindings")

# Create the addon
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

# Add node-addon-api and node includes to target
target_include_directories(${PROJECT_NAME} BEFORE PRIVATE 
  "${CMAKE_CURRENT_SOURCE_DIR}/src/compat"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/compat/private"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/compat/saucer"
  "${CMAKE_CURRENT_BINARY_DIR}/_deps/saucer-desktop-src/include"
  "${CMAKE_CURRENT_BINARY_DIR}/_deps/saucer-pdf-src/include"
  "${CMAKE_CURRENT_BINARY_DIR}/_deps/saucer-loop-src/include"
  ${CMAKE_JS_INC}
  ${NODE_INCLUDE_DIR}
  ${UV_INCLUDE_DIR}
  "${CMAKE_CURRENT_SOURCE_DIR}/node_modules/node-addon-api"
)

# Define static macros for proper symbol resolution
target_compile_definitions(${PROJECT_NAME} PRIVATE
  SAUCER_BINDINGS_STATIC_DEFINE
  SAUCER_BINDINGS_DESKTOP_STATIC_DEFINE
  SAUCER_BINDINGS_PDF_STATIC_DEFINE
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
  ${CMAKE_JS_LIB}
  saucer-bindings
  saucer-bindings-desktop
  saucer-bindings-pdf
  saucer-bindings-loop
  saucer-desktop
  saucer-pdf
  saucer-loop
)

# Platform-specific settings
if(APPLE)
  target_link_libraries(${PROJECT_NAME} "-framework WebKit" "-framework Cocoa" "-framework CoreFoundation")
elseif(UNIX)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(WEBKIT2 REQUIRED webkit2gtk-4.0)
  target_include_directories(${PROJECT_NAME} PRIVATE ${WEBKIT2_INCLUDE_DIRS})
  target_link_libraries(${PROJECT_NAME} ${WEBKIT2_LIBRARIES})
elseif(WIN32)
  target_link_libraries(${PROJECT_NAME} "WebView2LoaderStatic.lib")
endif()
