# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([mountewf], [0.1.0], [Daniel.GILLEN@police.etat.lu])
AM_INIT_AUTOMAKE(@PACKAGE_NAME@, @PACKAGE_VERSION@)

# Checks for programs.
AC_PROG_CC
AM_PROG_CC_C_O

# Checks for libraries.
AC_CHECK_LIB([fuse],[fuse_main])
AC_CHECK_LIB([ewf],[libewf_open])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdint.h stdlib.h string.h fuse.h libewf.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memset])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
