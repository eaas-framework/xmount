if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_morphing_raid C)

add_library(xmount_morphing_raid SHARED libxmount_morphing_raid.c ../../libxmount/libxmount.c)

install(TARGETS xmount_morphing_raid DESTINATION lib/xmount)

