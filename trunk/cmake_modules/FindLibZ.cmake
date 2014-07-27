find_package(PkgConfig)
pkg_check_modules(PC_LIBZ QUIET zlib)
set(LIBZ_DEFINITIONS ${PC_LIBZ_CFLAGS_OTHER})

find_path(LIBZ_INCLUDE_DIR zlib.h
          HINTS ${PC_LIBZ_INCLUDEDIR} ${PC_LIBZ_INCLUDE_DIRS}
          PATH_SUFFIXES zlib)

find_library(LIBZ_LIBRARY NAMES z libz
             HINTS ${PC_LIBZ_LIBDIR} ${PC_LIBZ_LIBRARY_DIRS} )

set(LIBZ_LIBRARIES ${LIBZ_LIBRARY})
set(LIBZ_INCLUDE_DIRS ${LIBZ_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibZ DEFAULT_MSG LIBZ_LIBRARY LIBZ_INCLUDE_DIR)

mark_as_advanced(LIBZ_INCLUDE_DIR LIBZ_LIBRARY)
