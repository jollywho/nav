cmake_minimum_required(VERSION 3.3.1)
project(fnav)
set(CMAKE_C_FLAGS "-g")
add_definitions("-Wall -Wextra -Wno-unused-parameter")

set( CMAKE_EXPORT_COMPILE_COMMANDS 1 )
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckSymbolExists)

include_directories("${PROJECT_SOURCE_DIR}/src")

file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/fnav/*.c")

add_executable(fnav ${SOURCES})

set(CURSES_NEED_WIDE true)
find_package(Curses REQUIRED)
find_package(LibTermkey REQUIRED)
find_package(LibUV REQUIRED)
target_link_libraries(fnav ${LIBUV_LIBRARIES})
target_link_libraries(fnav ${LIBTERMKEY_LIBRARIES})
target_link_libraries(fnav ${CURSES_LIBRARY})

set_target_properties(fnav PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
