#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)
AC_INIT(icoutils, 0.29.1, frank.richter@gmail.com)
AC_CONFIG_MACRO_DIR([m4]) # doesn't seem to have any effect at the moment
AC_CONFIG_SRCDIR([icotool/icotool.h])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

# Stuff for the config.h.in file
AC_GNU_SOURCE
AC_DEFINE_DIR(LOCALEDIR, datadir/locale, [Directory where gettext translations are kept.])
#AH_TEMPLATE([PKGUSERDIR], [User directory for program files])

# Checks for programs.
AC_PROG_CC
AC_PROG_LN_S
AC_PROG_RANLIB
# AC_PROG_LIBTOOL

# Gnulib
gl_EARLY
gl_INIT

# Gettext
AM_GNU_GETTEXT([external])
# AM_GNU_GETTEXT_VERSION(0.14.1)

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_HEADER_TIME
#AC_CHECK_HEADERS([byteswap.h errno.h fcntl.h getopt.h limits.h sys/stat.h sys/time.h sys/types.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_CONST
AC_C_BIGENDIAN
#AC_TYPE_OFF_T
#AC_TYPE_SIZE_T
#AC_TYPE_MODE_T
AC_CHECK_TYPES([comparison_fn_t])

# Checks for library functions.
AC_FUNC_FORK
AC_CHECK_FUNCS([pow])

# Check for libpng
AC_CHECK_LIB(png, png_create_read_struct, [
AC_SUBST(PNG_LIBS, "-lpng -lz -lm")
], [
  AC_CHECK_LIB(png10, png_create_read_struct, [
  AC_SUBST(PNG_LIBS, "-lpng10 -lz -lm")
  ], [
    AC_CHECK_LIB(png12, png_create_read_struct, [
    AC_SUBST(PNG_LIBS, "-lpng12 -lz -lm")
    ], [
      AC_MSG_ERROR([cannot find required library png])
    ], [-lz -lm])
  ], [-lz -lm])
], [-lz -lm])
AC_CHECK_HEADERS([png.h libpng/png.h libpng10/png.h libpng12/png.h])

AC_CONFIG_FILES([Makefile
		 icoutils.spec
		 po/Makefile.in
		 lib/Makefile
		 common/Makefile
		 icotool/Makefile
		 wrestool/Makefile
		 extresso/Makefile])
AC_CONFIG_FILES([extresso/extresso], [chmod +x extresso/extresso])
AC_CONFIG_FILES([extresso/genresscript], [chmod +x extresso/genresscript])
AC_OUTPUT
