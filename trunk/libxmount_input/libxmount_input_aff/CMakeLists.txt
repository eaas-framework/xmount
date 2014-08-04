if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_input_aff C)

add_library(xmount_input_aff SHARED libxmount_input_aff.c)
include_directories(${LIBAFF_INCLUDE_DIRS})
include_directories(${OPENSSL_INCLUDE_DIRS})
set(LIBS ${LIBS} ${LIBAFF_LIBRARIES})
set(LIBS ${LIBS} ${OPENSSL_LIBRARIES})
target_link_libraries(xmount_input_aff ${LIBS})

install(TARGETS xmount_input_aff DESTINATION lib/xmount)

