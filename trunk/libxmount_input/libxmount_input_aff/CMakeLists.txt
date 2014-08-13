if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_input_aff C)

add_library(xmount_input_aff SHARED libxmount_input_aff.c)

if(NOT STATIC_AFF)
  include_directories(${LIBAFF_INCLUDE_DIRS})
  include_directories(${OPENSSL_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBAFF_LIBRARIES})
  set(LIBS ${LIBS} ${OPENSSL_LIBRARIES})
else(NOT STATIC_AFF)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libaff/lib)
  set(LIBS ${LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/libaff/lib/.libs/libafflib.a)
  set(LIBS ${LIBS} "-lstdc++ -lcrypto -lssl -lrt -lz")
  add_definitions(-DHAVE_LIBAFF_STATIC)
endif(NOT STATIC_AFF)

target_link_libraries(xmount_input_aff ${LIBS})

install(TARGETS xmount_input_aff DESTINATION lib/xmount)

