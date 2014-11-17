if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_morphing_combine C)

add_library(xmount_morphing_combine SHARED libxmount_morphing_combine.c ../../libxmount/libxmount.c)

install(TARGETS xmount_morphing_combine DESTINATION lib/xmount)

