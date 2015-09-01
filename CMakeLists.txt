cmake_minimum_required(VERSION 3.3.1)
project(fnav)

set( CMAKE_EXPORT_COMPILE_COMMANDS 1 )
list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -Wall -Wextra -Werror-implicit-function-declaration -Wno-unused-parameter")

include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckSymbolExists)

add_subdirectory(src/fnav)
