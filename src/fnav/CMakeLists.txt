set(SOURCES
  fnav.c
  controller.c
  event.c
  parser.c)

add_executable(fnav ${SOURCES})

target_link_libraries(fnav ${LIBUV_LIBRARY})

set_target_properties(fnav PROPERTIES
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
