if(POLICY CMP0042)
  cmake_policy(SET CMP0042 NEW) # CMake 3.0
endif(POLICY CMP0042)

project(libxmount_input_aewf C)

add_library(xmount_input_aewf SHARED libxmount_input_aewf.c ../../libxmount/libxmount.c)

include_directories(${LIBZ_INCLUDE_DIRS})

target_link_libraries(xmount_input_aewf ${LIBZ_LIBRARIES})

install(TARGETS xmount_input_aewf DESTINATION lib/xmount)

