# TODO: https://www.mail-archive.com/cmake@cmake.org/msg32933.html

cmake_minimum_required(VERSION 2.8)
project(libxmount_input_aff)
add_library(xmount_input_aff SHARED libxmount_input_aff.c)
install(TARGETS xmount_input_aff DESTINATION lib/xmount)
include_directories(${LIBAFF_INCLUDE_DIRS})
set(LIBS ${LIBS} ${LIBAFF_LIBRARIES})
target_link_libraries(xmount_input_aff ${LIBS})
