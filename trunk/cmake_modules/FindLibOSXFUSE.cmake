find_package(PkgConfig)
pkg_check_modules(PC_LIBOSXFUSE QUIET libosxfuse)
set(LIBOSXFUSE_DEFINITIONS ${PC_LIBOSXFUSE_CFLAGS_OTHER})

find_path(LIBOSXFUSE_INCLUDE_DIR fuse.h
          HINTS ${PC_LIBOSXFUSE_INCLUDEDIR} ${PC_LIBOSXFUSE_INCLUDE_DIRS}
          PATH_SUFFIXES osxfuse)

find_library(LIBOSXFUSE_LIBRARY NAMES osxfuse libosxfuse
             HINTS ${PC_LIBOSXFUSE_LIBDIR} ${PC_LIBOSXFUSE_LIBRARY_DIRS})

set(LIBOSXFUSE_LIBRARIES ${LIBOSXFUSE_LIBRARY})
set(LIBOSXFUSE_INCLUDE_DIRS ${LIBOSXFUSE_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibOSXFUSE
                                  DEFAULT_MSG
                                  LIBOSXFUSE_LIBRARY
                                  LIBOSXFUSE_INCLUDE_DIR)

mark_as_advanced(LIBOSXFUSE_INCLUDE_DIR LIBOSXFUSE_LIBRARY)

