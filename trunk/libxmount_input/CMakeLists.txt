set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules/")

find_package(LibEWF)
if(LIBEWF_FOUND)
  add_subdirectory(libxmount_input_ewf)
endif(LIBEWF_FOUND)

find_package(LibAFF)
if(LIBAFF_FOUND)
  add_subdirectory(libxmount_input_aff)
endif(LIBAFF_FOUND)

