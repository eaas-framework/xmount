# Try pkg-config first
find_package(PkgConfig)
pkg_check_modules(PKGC_LIBAFF QUIET afflib)

if(PKGC_LIBAFF_FOUND)
  # Found lib using pkg-config.
  if(CMAKE_DEBUG)
    message(STATUS "\${PKGC_LIBAFF_LIBRARIES} = ${PKGC_LIBAFF_LIBRARIES}")
    message(STATUS "\${PKGC_LIBAFF_LIBRARY_DIRS} = ${PKGC_LIBAFF_LIBRARY_DIRS}")
    message(STATUS "\${PKGC_LIBAFF_LDFLAGS} = ${PKGC_LIBAFF_LDFLAGS}")
    message(STATUS "\${PKGC_LIBAFF_LDFLAGS_OTHER} = ${PKGC_LIBAFF_LDFLAGS_OTHER}")
    message(STATUS "\${PKGC_LIBAFF_INCLUDE_DIRS} = ${PKGC_LIBAFF_INCLUDE_DIRS}")
    message(STATUS "\${PKGC_LIBAFF_CFLAGS} = ${PKGC_LIBAFF_CFLAGS}")
    message(STATUS "\${PKGC_LIBAFF_CFLAGS_OTHER} = ${PKGC_LIBAFF_CFLAGS_OTHER}")
  endif(CMAKE_DEBUG)

  set(LIBAFF_LIBRARIES ${PKGC_LIBAFF_LIBRARIES})
  set(LIBAFF_INCLUDE_DIRS ${PKGC_LIBAFF_INCLUDE_DIRS})
  #set(LIBAFF_DEFINITIONS ${PKGC_LIBAFF_CFLAGS_OTHER})
else(PKGC_LIBAFF_FOUND)
  # Didn't find lib using pkg-config. Try to find it manually
  message(WARNING "Unable to find LibAFF using pkg-config! If compilation fails, make sure pkg-config is installed and PKG_CONFIG_PATH is set correctly")

  find_path(LIBAFF_INCLUDE_DIR afflib/afflib.h
            PATH_SUFFIXES afflib)
  find_library(LIBAFF_LIBRARY NAMES afflib libafflib)

  if(CMAKE_DEBUG)
    message(STATUS "\${LIBAFF_LIBRARY} = ${LIBAFF_LIBRARY}")
    message(STATUS "\${LIBAFF_INCLUDE_DIR} = ${LIBAFF_INCLUDE_DIR}")
  endif(CMAKE_DEBUG)

  set(LIBAFF_LIBRARIES ${LIBAFF_LIBRARY})
  set(LIBAFF_INCLUDE_DIRS ${LIBAFF_INCLUDE_DIR})
endif(PKGC_LIBAFF_FOUND)

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set <PREFIX>_FOUND to TRUE if
# all listed variables are TRUE
find_package_handle_standard_args(LibAFF DEFAULT_MSG LIBAFF_LIBRARIES)

