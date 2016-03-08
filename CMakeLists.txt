cmake_minimum_required(VERSION 3.3.1)
project(nav)
set(CMAKE_C_FLAGS "-g")
add_definitions("-std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers -Wno-sign-compare")
add_definitions(-D_GNU_SOURCE)

set( CMAKE_EXPORT_COMPILE_COMMANDS 1 )
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckSymbolExists)

include_directories("${PROJECT_SOURCE_DIR}/src")

file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/nav/*.c")

add_executable(nav ${SOURCES})

set(CURSES_NEED_WIDE true)
find_package(Curses REQUIRED)
find_package(PCRE REQUIRED)
find_package(LibUV REQUIRED)
find_package(LibTermkey REQUIRED)
target_link_libraries(nav ${CURSES_LIBRARY})
target_link_libraries(nav ${PCRE_LIBRARY})
target_link_libraries(nav ${LIBUV_LIBRARIES})
target_link_libraries(nav ${LIBTERMKEY_LIBRARIES})
target_link_libraries(nav util)

set_target_properties(nav PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
