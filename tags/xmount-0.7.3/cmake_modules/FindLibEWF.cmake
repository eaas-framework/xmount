# Try pkg-config first
find_package(PkgConfig)
pkg_check_modules(PKGC_LIBEWF QUIET libewf)

if(PKGC_LIBEWF_FOUND)
  # Found lib using pkg-config.
  if(CMAKE_DEBUG)
    message(STATUS "\${PKGC_LIBEWF_LIBRARIES} = ${PKGC_LIBEWF_LIBRARIES}")
    message(STATUS "\${PKGC_LIBEWF_LIBRARY_DIRS} = ${PKGC_LIBEWF_LIBRARY_DIRS}")
    message(STATUS "\${PKGC_LIBEWF_LDFLAGS} = ${PKGC_LIBEWF_LDFLAGS}")
    message(STATUS "\${PKGC_LIBEWF_LDFLAGS_OTHER} = ${PKGC_LIBEWF_LDFLAGS_OTHER}")
    message(STATUS "\${PKGC_LIBEWF_INCLUDE_DIRS} = ${PKGC_LIBEWF_INCLUDE_DIRS}")
    message(STATUS "\${PKGC_LIBEWF_CFLAGS} = ${PKGC_LIBEWF_CFLAGS}")
    message(STATUS "\${PKGC_LIBEWF_CFLAGS_OTHER} = ${PKGC_LIBEWF_CFLAGS_OTHER}")
  endif(CMAKE_DEBUG)

  set(LIBEWF_LIBRARIES ${PKGC_LIBEWF_LIBRARIES})
  set(LIBEWF_INCLUDE_DIRS ${PKGC_LIBEWF_INCLUDE_DIRS})
  #set(LIBEWF_DEFINITIONS ${PKGC_LIBEWF_CFLAGS_OTHER})
else(PKGC_LIBEWF_FOUND)
  # Didn't find lib using pkg-config. Try to find it manually
  message(WARNING "Unable to find LibEWF using pkg-config! If compilation fails, make sure pkg-config is installed and PKG_CONFIG_PATH is set correctly")

  find_path(LIBEWF_INCLUDE_DIR libewf.h
            PATH_SUFFIXES libewf)
  find_library(LIBEWF_LIBRARY NAMES ewf libewf)

  if(CMAKE_DEBUG)
    message(STATUS "\${LIBEWF_LIBRARY} = ${LIBEWF_LIBRARY}")
    message(STATUS "\${LIBEWF_INCLUDE_DIR} = ${LIBEWF_INCLUDE_DIR}")
  endif(CMAKE_DEBUG)

  set(LIBEWF_LIBRARIES ${LIBEWF_LIBRARY})
  set(LIBEWF_INCLUDE_DIRS ${LIBEWF_INCLUDE_DIR})
endif(PKGC_LIBEWF_FOUND)

include(FindPackageHandleStandardArgs)
# Handle the QUIETLY and REQUIRED arguments and set <PREFIX>_FOUND to TRUE if
# all listed variables are TRUE
find_package_handle_standard_args(LibEWF DEFAULT_MSG LIBEWF_LIBRARIES)

