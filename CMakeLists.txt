cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0042 NEW)

project(saucer-nodejs LANGUAGES CXX)

# Enable Objective-C++ for macOS
if(APPLE)
  enable_language(OBJCXX)
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

# Get Node.js include directory (contains N-API/Node headers)
execute_process(
  COMMAND node -p "process.config.variables.node_prefix + '/include/node'"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE NODE_INCLUDE_DIR
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Node.js include directory: ${NODE_INCLUDE_DIR}")

# Locate libuv headers (Node may not ship uv.h in node headers)
find_path(UV_INCLUDE_DIR uv.h
  HINTS
    ${NODE_INCLUDE_DIR}
    ${NODE_INCLUDE_DIR}/..
    /opt/homebrew/include
    /usr/local/include
    /usr/include
)

if (NOT UV_INCLUDE_DIR)
  message(FATAL_ERROR "Could not locate uv.h - install libuv headers or set UV_INCLUDE_DIR")
else()
  message(STATUS "libuv include directory: ${UV_INCLUDE_DIR}")
endif()

# Include directories
include_directories(${CMAKE_JS_INC})
include_directories(${NODE_INCLUDE_DIR})
include_directories(${UV_INCLUDE_DIR})
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/bindings/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/bindings/modules/desktop/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/vendor/bindings/modules/pdf/include")

# Source files
file(GLOB SOURCE_FILES "src/*.cpp")

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
set(saucer_prefer_remote OFF CACHE BOOL "Prefer remote saucer packages" FORCE)
set(saucer_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../saucer" CACHE STRING "Use local saucer sources" FORCE)
set(CPM_USE_LOCAL_PACKAGES ON CACHE BOOL "Use local CPM packages when available" FORCE)

# Add vendored saucer bindings as a subdirectory
# The bindings will automatically fetch the saucer library from GitHub via CPM
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/vendor/bindings" "${CMAKE_CURRENT_BINARY_DIR}/bindings")

# Create the addon
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

# Add node-addon-api and node includes to target
target_include_directories(${PROJECT_NAME} PRIVATE 
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
