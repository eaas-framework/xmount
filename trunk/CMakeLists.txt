cmake_minimum_required(VERSION 2.8)
project(xmount C)

include(CheckIncludeFiles)
#include(CheckCSourceCompiles)

# Make sure CMAKE_BUILD_TYPE is set
if(CMAKE_BUILD_TYPE STREQUAL "")
  set(CMAKE_BUILD_TYPE "Release")
else(CMAKE_BUILD_TYPE STREQUAL "")
  if(NOT (CMAKE_BUILD_TYPE STREQUAL "Release" OR
          CMAKE_BUILD_TYPE STREQUAL "Debug"))
    message(FATAL_ERROR "Only build types 'Release' and 'Debug' are supported!")
  endif(NOT (CMAKE_BUILD_TYPE STREQUAL "Release" OR
             CMAKE_BUILD_TYPE STREQUAL "Debug"))
endif(CMAKE_BUILD_TYPE STREQUAL "")

# Add cmake_modules dir to CMAKE_MODULE_PATH
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules/")

# Check required headers
check_include_files(stdlib.h HAVE_STDLIB_H)
check_include_files(stdio.h HAVE_STDIO_H)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(stdarg.h HAVE_STDARG_H)
check_include_files(string.h HAVE_STRING_H)
check_include_files(errno.h HAVE_ERRNO_H)
check_include_files(fcntl.h HAVE_FCNTL_H)
check_include_files(dlfcn.h HAVE_DLFCN_H)
check_include_files(dirent.h HAVE_DIRENT_H)
check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files(sys/ioctl.h HAVE_SYS_IOCTL_H)
check_include_files(sys/types.h HAVE_SYS_TYPES_H)
check_include_files(linux/fs.h HAVE_LINUX_FS_H)
check_include_files(pthread.h HAVE_PTHREAD_H)
check_include_files(time.h HAVE_TIME_H)
check_include_files(inttypes.h HAVE_INTTYPES_H)
check_include_files(byteswap.h HAVE_BYTESWAP_H)
check_include_files(endian.h HAVE_ENDIAN_H)
check_include_files(libkern/OSByteOrder.h HAVE_LIBKERN_OSBYTEORDER_H)

# Check for required libs
if(NOT APPLE)
  find_package(LibFUSE REQUIRED)
  include_directories(${LIBFUSE_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBFUSE_LIBRARIES})
else(NOT APPLE)
  find_package(LibOSXFUSE REQUIRED)
  include_directories(${LIBOSXFUSE_INCLUDE_DIRS})
  set(LIBS ${LIBS} ${LIBOSXFUSE_LIBRARIES})
endif(NOT APPLE)

# Generate config.h and add it's path to the include dirs
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# Add preprocessor definitions
add_definitions(-D_LARGEFILE64_SOURCE)
add_definitions(-D_FILE_OFFSET_BITS=64)
add_definitions(-D_GNU_SOURCE)
set(CMAKE_C_FLAGS "-fno-strict-aliasing -std=c99 -Wall")
set(CMAKE_C_FLAGS_RELEASE "-O2 ${CMAKE_C_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG "-ggdb -O0 ${CMAKE_C_FLAGS}")

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

# Compile stuff in sub dirs
add_subdirectory(libxmount_input)
add_subdirectory(src)

