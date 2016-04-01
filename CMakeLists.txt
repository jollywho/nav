cmake_minimum_required(VERSION 3.3.1)
project(nav)
set(CMAKE_C_FLAGS "-g")
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-parameter")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-sign-compare")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-missing-field-initializers")
add_definitions(-D_GNU_SOURCE)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
  execute_process(
  COMMAND git describe --always
  OUTPUT_VARIABLE GIT_COMMIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)
endif()
add_definitions(-DNAV_VERSION=\"${GIT_COMMIT_HASH}\")

string(TIMESTAMP CURRENT_DATE "%Y-%m-%d" UTC)
add_definitions(-DNAV_DATE=\"${CURRENT_DATE}\")

set( CMAKE_EXPORT_COMPILE_COMMANDS ON )
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckSymbolExists)

include_directories("${PROJECT_SOURCE_DIR}/src")

file(GLOB_RECURSE SOURCES "${PROJECT_SOURCE_DIR}/src/nav/*.c")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/build)

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

find_program(HAS_W3M w3m)
if (HAS_W3M)
  add_definitions(-DW3M_SUPPORTED=1)
endif()


install(
  TARGETS nav
  RUNTIME
  DESTINATION bin
  COMPONENT runtime
)
install(
  FILES ${PROJECT_SOURCE_DIR}/navrc
  DESTINATION /etc/nav/
  COMPONENT configuration
)
install(
  FILES ${PROJECT_SOURCE_DIR}/nav.1
  DESTINATION share/man/man1
  COMPONENT documentation
)
