#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
LT_PREREQ([2.2.0])

AC_INIT(blockbench, 0.3, elm@cs.ucsc.edu)
AM_INIT_AUTOMAKE(blockbench, 0.3)

LT_INIT([disable-shared])

AC_CONFIG_SRCDIR([src/ioload/iol_main.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_MAKE_SET
AC_CONFIG_MACRO_DIR([m4])

# Checks for libraries.
AC_CHECK_LIB([rt], [sem_init], [], AC_MSG_ERROR('librt not found'))
AC_CHECK_LIB([pthread], [pthread_create], [], AC_MSG_ERROR('libpthread not found'))

AC_SEARCH_LIBS([clock_gettime gethrtime], [rt])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h inttypes.h limits.h netinet/in.h stdint.h stdlib.h string.h sys/ioctl.h sys/param.h sys/time.h unistd.h])
AC_CHECK_HEADER([scsi/sg_lib.h], [], AC_MSG_ERROR('include files from sg3_utils not found'))

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_STRTOD
AC_CHECK_FUNCS([clock_gettime gethrtime])
AC_CHECK_FUNCS([memset strdup strerror strtoul strtoull])

AC_CONFIG_FILES([Makefile
                 lib/Makefile
                 src/iofill/Makefile
                 src/ioload/Makefile])
AC_OUTPUT
