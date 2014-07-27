add_subdirectory(libxmount_input_dd)

find_package(LibEWF)
if(LIBEWF_FOUND)
  add_subdirectory(libxmount_input_ewf)
endif(LIBEWF_FOUND)

#find_package(LibAFF)
#if(LIBAFF_FOUND)
#  add_subdirectory(libxmount_input_aff)
#endif(LIBAFF_FOUND)

find_package(LibZ)
if(LIBZ_FOUND)
  add_subdirectory(libxmount_input_aewf)
  add_subdirectory(libxmount_input_aaff)
endif(LIBZ_FOUND)

