find_package(PkgConfig)
pkg_check_modules(PC_LIBAFF QUIET afflib)
set(LIBAFF_DEFINITIONS ${PC_LIBAFF_CFLAGS_OTHER})

find_path(LIBAFF_INCLUDE_DIR afflib/afflib.h
          HINTS ${PC_LIBAFF_INCLUDEDIR} ${PC_LIBAFF_INCLUDE_DIRS}
          PATH_SUFFIXES afflib)

find_library(LIBAFF_LIBRARY NAMES afflib libafflib
             HINTS ${PC_LIBAFF_LIBDIR} ${PC_LIBAFF_LIBRARY_DIRS} )

set(LIBAFF_LIBRARIES ${LIBAFF_LIBRARY})
set(LIBAFF_INCLUDE_DIRS ${LIBAFF_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LIBXML2_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(LibAFF DEFAULT_MSG LIBAFF_LIBRARY LIBAFF_INCLUDE_DIR)

mark_as_advanced(LIBAFF_INCLUDE_DIR LIBAFF_LIBRARY)

