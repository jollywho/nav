set(SOURCES
  fnav.c
  controller.c
  event.c
  parser.c)

add_executable(fnav ${SOURCES})

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
