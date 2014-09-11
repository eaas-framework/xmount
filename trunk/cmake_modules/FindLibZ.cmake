# Try pkg-config first
find_package(PkgConfig)
pkg_check_modules(PKGC_LIBZ QUIET zlib)

if(PKGC_LIBZ_FOUND)
  # Found lib using pkg-config.
  if(CMAKE_DEBUG)
    message(STATUS "\${PKGC_LIBZ_LIBRARIES} = ${PKGC_LIBZ_LIBRARIES}")
    message(STATUS "\${PKGC_LIBZ_LIBRARY_DIRS} = ${PKGC_LIBZ_LIBRARY_DIRS}")
    message(STATUS "\${PKGC_LIBZ_LDFLAGS} = ${PKGC_LIBZ_LDFLAGS}")
    message(STATUS "\${PKGC_LIBZ_LDFLAGS_OTHER} = ${PKGC_LIBZ_LDFLAGS_OTHER}")
    message(STATUS "\${PKGC_LIBZ_INCLUDE_DIRS} = ${PKGC_LIBZ_INCLUDE_DIRS}")
    message(STATUS "\${PKGC_LIBZ_CFLAGS} = ${PKGC_LIBZ_CFLAGS}")
    message(STATUS "\${PKGC_LIBZ_CFLAGS_OTHER} = ${PKGC_LIBZ_CFLAGS_OTHER}")
  endif(CMAKE_DEBUG)

  set(LIBZ_LIBRARIES ${PKGC_LIBZ_LIBRARIES})
  set(LIBZ_INCLUDE_DIRS ${PKGC_LIBZ_INCLUDE_DIRS})
  #set(LIBZ_DEFINITIONS ${PKGC_LIBZ_CFLAGS_OTHER})
else(PKGC_LIBZ_FOUND)
  # Didn't find lib using pkg-config. Try to find it manually
  message(WARNING "Unable to find LibZ using pkg-config! If compilation fails, make sure pkg-config is installed and PKG_CONFIG_PATH is set correctly")

  find_path(LIBZ_INCLUDE_DIR zlib.h
            PATH_SUFFIXES zlib)
  find_library(LIBZ_LIBRARY NAMES z libz)

  if(CMAKE_DEBUG)
    message(STATUS "\${LIBZ_LIBRARY} = ${LIBZ_LIBRARY}")
    message(STATUS "\${LIBZ_INCLUDE_DIR} = ${LIBZ_INCLUDE_DIR}")
  endif(CMAKE_DEBUG)

  set(LIBZ_LIBRARIES ${LIBZ_LIBRARY})
  set(LIBZ_INCLUDE_DIRS ${LIBZ_INCLUDE_DIR})
endif(PKGC_LIBZ_FOUND)

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set <PREFIX>_FOUND to TRUE if
# all listed variables are TRUE
find_package_handle_standard_args(LibZ DEFAULT_MSG LIBZ_LIBRARIES)

