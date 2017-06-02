
AC_PREREQ(2.59)

AC_INIT([libsoot],[0.99.00])

AC_CONFIG_AUX_DIR([m4])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_FILES(Makefile src/Makefile src/soot/Makefile src/tests/Makefile third_party/Makefile)

AM_INIT_AUTOMAKE()

m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

LT_INIT([pic-only])

dnl # In these next two lines we prevent the building of
dnl # shared libraries and prevent the installation of things
dnl # like headers or the static soot library. We do this
dnl # so that developers don't get the wrong idea---this library
dnl # is only intended to be used by SiLabs chips and shouldn't
dnl # be considered for general-purpose use.
AC_DISABLE_SHARED
AM_CONDITIONAL([ALLOW_INSTALL],[false])

AC_PROG_LIBTOOL
AC_CONFIG_HEADERS([src/config.h])

AM_MAINTAINER_MODE

AX_CODE_COVERAGE

AC_PROG_CC([],[AC_MSG_ERROR(["A working C compiler is required"])])

AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug],
			[enable compiling with debugging information]), [
	if (test "${enableval}" = "yes" &&
				test "${ac_cv_prog_cc_g}" = "yes"); then
		CFLAGS="$CFLAGS -g -DDEBUG=1"
		CXXFLAGS="$CXXFLAGS -g -DDEBUG=1"
	fi
])

AC_DEFINE_UNQUOTED([ASSERT_MACROS_USE_SYSLOG], 1, [Define to 1 to have assertmacros.h use syslog])
AC_DEFINE_UNQUOTED([ASSERT_MACROS_SQUELCH], 0, [Define to 0 to explicitly prevent squelching assert printouts])

AC_OUTPUT
