AC_PREREQ([2.69])
AC_INIT([darc2json], [0.2-SNAPSHOT], [oona@kapsi.fi], [darc2json],
        [https://github.com/windytan/darc2json])
AM_INIT_AUTOMAKE([1.10 -Wall -Werror foreign subdir-objects])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
 Makefile
 src/Makefile
])

AC_ARG_WITH([liquid],
            [AS_HELP_STRING([--without-liquid],
              [disable support for demodulation using liquid-dsp])])

AC_ARG_WITH([sndfile],
            [AS_HELP_STRING([--without-sndfile],
               [disable support for reading audio files via libsndfile])])

AC_ARG_WITH([macports],
            [AS_HELP_STRING([--without-macports],
               [disable check for MacPorts directories when compiling])])

LIQUID=
AS_IF([test "x$with_liquid" != xno],
  [AC_CHECK_LIB([liquid], [symtrack_cccf_create],
    [AC_SUBST([LIQUID], ["-lliquid"])
     AC_DEFINE([HAVE_LIQUID], [1],
               [Define if you have liquid])
    ],
    [AC_MSG_FAILURE(
       [Could not find liquid-dsp (use --without-liquid to disable)])])])

MACPORTS_LD=
AS_IF([test "x$with_macports" != xno],
  [AC_CHECK_FILE(/opt/local/lib,
    [AC_SUBST([MACPORTS_LD], ["-L/opt/local/lib"])])])

MACPORTS_CF=
AS_IF([test "x$with_macports" != xno],
  [AC_CHECK_FILE(/opt/local/lib,
    [AC_SUBST([MACPORTS_CF], ["-I/opt/local/include"])])])

SNDFILE=
AS_IF([test "x$with_sndfile" != xno],
  [AC_CHECK_LIB([sndfile], [main],
    [AC_SUBST([SNDFILE], ["-lsndfile"])
     AC_DEFINE([HAVE_SNDFILE], [1],
               [Define if you have libsndfile])
    ],
    [AC_MSG_FAILURE(
       [Could not find libsndfile (use --without-sndfile to disable)])])])

AC_PROG_CXX
AC_PROG_RANLIB
AC_LANG([C++])

AC_OUTPUT
