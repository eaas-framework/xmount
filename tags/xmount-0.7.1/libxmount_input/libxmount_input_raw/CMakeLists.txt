if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_input_raw C)

add_library(xmount_input_raw SHARED libxmount_input_raw.c ../../libxmount/libxmount.c)

install(TARGETS xmount_input_raw DESTINATION lib/xmount)

