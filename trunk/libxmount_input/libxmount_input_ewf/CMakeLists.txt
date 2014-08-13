if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_input_ewf C)

add_library(xmount_input_ewf SHARED libxmount_input_ewf.c)

if(NOT STATIC_EWF)
  include_directories(${LIBEWF_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBEWF_LIBRARIES})
else(NOT STATIC_EWF)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR}/libewf/include)
  set(LIBS ${LIBS} ${CMAKE_CURRENT_SOURCE_DIR}/libewf/libewf/.libs/libewf.a)
  set(LIBS ${LIBS} -lz)
  add_definitions(-DHAVE_LIBEWF_STATIC)
endif(NOT STATIC_EWF)

target_link_libraries(xmount_input_ewf ${LIBS})

install(TARGETS xmount_input_ewf DESTINATION lib/xmount)

