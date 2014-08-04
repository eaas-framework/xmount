if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_input_dd C)

add_library(xmount_input_dd SHARED libxmount_input_dd.c)

install(TARGETS xmount_input_dd DESTINATION lib/xmount)

