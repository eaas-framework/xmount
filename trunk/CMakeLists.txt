include(CheckCSourceCompiles)

cmake_minimum_required(VERSION 2.8)

add_definitions(-DXMOUNT_VERSION="0.7.0")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules/")
set(CMAKE_C_FLAGS "-ggdb -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -std=c99 -Wall")

# Check that off_t can represent 2**63 - 1 correctly.
# If it can't, we need to set _FILE_OFFSET_BITS=64
#check_c_source_compiles("
#  #include <sys/types.h>
#  #define LARGE_OFF_T (((off_t) 1 << 62) - 1 + ((off_t) 1 << 62))
#  int off_t_is_large[(LARGE_OFF_T % 2147483629 == 721 && LARGE_OFF_T % 2147483647 == 1) ? 1 : -1];
#  int main() { return 0; }
#" _OFFT_IS_64BIT)
#if(NOT ${_OFFT_IS_64BIT})
#  set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} "-D_FILE_OFFSET_BITS=64")
#endif(NOT ${_OFFT_IS_64BIT})

add_subdirectory(libxmount_input)
add_subdirectory(src)
