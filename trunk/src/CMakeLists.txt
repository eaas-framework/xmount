project(xmount)

add_definitions(-DXMOUNT_VERSION="0.7.0")

if(NOT APPLE)
  include_directories(${LIBFUSE_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBFUSE_LIBRARIES})
else(NOT APPLE)
  include_directories(${LIBOSXFUSE_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBOSXFUSE_LIBRARIES})
endif(NOT APPLE)

set(LIBS ${LIBS} "dl")

#set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-fno-strict-aliasing")

add_definitions(-DXMOUNT_LIBRARY_PATH="${CMAKE_INSTALL_PREFIX}/lib/xmount")

add_executable(xmount xmount.c md5.c)

target_link_libraries(xmount ${LIBS})

install(TARGETS xmount DESTINATION bin)

