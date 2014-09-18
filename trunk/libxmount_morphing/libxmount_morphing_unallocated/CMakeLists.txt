if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_morphing_unallocated C)

add_library(xmount_morphing_unallocated SHARED libxmount_morphing_unallocated.c hfs_functions.c fat_functions.c ../../libxmount/libxmount.c)

install(TARGETS xmount_morphing_unallocated DESTINATION lib/xmount)

