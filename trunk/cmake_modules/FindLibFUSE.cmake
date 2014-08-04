find_package(PkgConfig)
pkg_check_modules(PC_LIBFUSE QUIET libfuse)
set(LIBFUSE_DEFINITIONS ${PC_LIBFUSE_CFLAGS_OTHER})

find_path(LIBFUSE_INCLUDE_DIR fuse.h
          HINTS ${PC_LIBFUSE_INCLUDEDIR} ${PC_LIBFUSE_INCLUDE_DIRS}
          PATH_SUFFIXES fuse)

find_library(LIBFUSE_LIBRARY NAMES fuse libfuse
             HINTS ${PC_LIBFUSE_LIBDIR} ${PC_LIBFUSE_LIBRARY_DIRS})

set(LIBFUSE_LIBRARIES ${LIBFUSE_LIBRARY})
set(LIBFUSE_INCLUDE_DIRS ${LIBFUSE_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibFUSE
                                  DEFAULT_MSG
                                  LIBFUSE_LIBRARY
                                  LIBFUSE_INCLUDE_DIR)

mark_as_advanced(LIBFUSE_INCLUDE_DIR LIBFUSE_LIBRARY)

