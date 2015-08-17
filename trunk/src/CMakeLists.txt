if(NOT APPLE)
  include_directories(${LIBFUSE_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBFUSE_LIBRARIES})
else(NOT APPLE)
  include_directories(${LIBOSXFUSE_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBOSXFUSE_LIBRARIES})
  link_directories(${LIBOSXFUSE_LIBRARY_DIRS})
endif(NOT APPLE)

set(LIBS ${LIBS} "dl")

add_definitions(-DXMOUNT_LIBRARY_PATH="${CMAKE_INSTALL_PREFIX}/lib/xmount")

add_executable(xmount xmount.c md5.c ../libxmount/libxmount.c)

target_link_libraries(xmount ${LIBS})

install(TARGETS xmount DESTINATION bin)

