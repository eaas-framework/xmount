find_package(PkgConfig)
pkg_check_modules(PC_LIBEWF QUIET libewf)
set(LIBEWF_DEFINITIONS ${PC_LIBEWF_CFLAGS_OTHER})

find_path(LIBEWF_INCLUDE_DIR libewf.h
          HINTS ${PC_LIBEWF_INCLUDEDIR} ${PC_LIBEWF_INCLUDE_DIRS}
          PATH_SUFFIXES libewf )

find_library(LIBEWF_LIBRARY NAMES ewf libewf
             HINTS ${PC_LIBEWF_LIBDIR} ${PC_LIBEWF_LIBRARY_DIRS} )

set(LIBEWF_LIBRARIES ${LIBEWF_LIBRARY})
set(LIBEWF_INCLUDE_DIRS ${LIBEWF_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibEWF DEFAULT_MSG LIBEWF_LIBRARY LIBEWF_INCLUDE_DIR)

mark_as_advanced(LIBEWF_INCLUDE_DIR LIBEWF_LIBRARY)
