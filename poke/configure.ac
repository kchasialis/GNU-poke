dnl configure.ac for GNU poke
dnl
dnl Please process this file with autoconf to get a 'configure'
dnl script.

dnl Copyright (C) 2019, 2020 Jose E. Marchesi

dnl This program is free software: you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation, either version 3 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see <http://www.gnu.org/licenses/>.

AC_INIT([GNU poke], [0.1-beta], [poke-devel@gnu.org], [poke],
        [http://www.jemarch.net/poke.html])

AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS(poke/config.h)
AC_CONFIG_MACRO_DIRS([m4])

# Include the Autoconf macros from Jitter.
m4_include([m4/jitter.m4])


dnl Autobuild
dnl AB_INIT

AC_DEFINE([PACKAGE_BUGZILLA],["https://sourceware.org/bugzilla/describecomponents.cgi?product=poke"],[URL for entering bugs in the project's bugzilla])

AC_PROG_CC
gl_EARLY
libpoke_EARLY
gui_EARLY

LT_INIT
AC_PROG_CC_C99
AM_PROG_CC_C_O

: ${LEX='flex'}
: ${LEXLIB='-lfl'}
: ${LEX_OUTPUT_ROOT='lex.yy'}
AC_ARG_VAR([LEX], [The flex implementation to use.])
AC_ARG_VAR([LEXLIB], [Options for linking with the flex runtime library.])
AC_ARG_VAR([LEX_OUTPUT_ROOT], [Base of the file name that the lexer generates.])

gl_BISON

dnl System
AC_CANONICAL_HOST
canonical=$host

gl_INIT
libpoke_INIT
gui_INIT

# Try to find a libtextstyle with hyperlinks support.
gl_LIBTEXTSTYLE_OPTIONAL([0.20.5])

# GNU help2man creates man pages from --help output; in many cases, this
# is sufficient, and obviates the need to maintain man pages separately.
# However, this means invoking executables, which we generally cannot do
# when cross-compiling, so we test to avoid that (the variable
# "cross_compiling" is set by AC_PROG_CC).
if test $cross_compiling = no; then
  AM_MISSING_PROG(HELP2MAN, help2man)
else
  HELP2MAN=:
fi

dnl i18n with gettext
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

# Poke requires a version of awk that supports gensub()
AX_CHECK_AWK_GENSUB([], AC_MSG_ERROR([$AWK does not support gensub.]))

dnl The Boehm-Weiser garbage collector

PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES(BDW_GC,[bdw-gc],[have_gc=yes],[have_gc=no])

dnl The JSON-C library for the machine-interface

PKG_CHECK_MODULES(JSON_C,[json-c],[have_json_c=yes],[have_json_c=no])
AC_ARG_ENABLE([mi],
              AS_HELP_STRING([--enable-mi],
                             [Enable the poke Machine-Interface (default is YES)]),
              [mi_enabled=$enableval], [mi_enabled=$have_json_c])
AM_CONDITIONAL([POKE_MI], [test "x$mi_enabled" = "xyes"])
if test "x$mi_enabled" = "xyes"; then
  AC_DEFINE([POKE_MI], 1,
            [Defined if building poke with the machine-interface enabled])
fi

dnl Jitter

AC_JITTER_SUBPACKAGE([jitter])

dnl libnbd for nbd:// io spaces (optional). Testing it also requires nbdkit
PKG_CHECK_MODULES([LIBNBD], [libnbd], [
  AC_SUBST([LIBNBD_CFLAGS])
  AC_SUBST([LIBNBD_LIBS])
  AC_DEFINE([HAVE_LIBNBD], [1], [libnbd found at compile time])
  libnbd_enabled=yes
  AC_PATH_PROGS([NBDKIT], [nbdkit], [no], [$PATH:/usr/local/sbin:/usr/sbin])
], [libnbd_enabled=no NBDKIT=no])
AM_CONDITIONAL([NBD], [test "x$libnbd_enabled" = "xyes"])

dnl Used in Makefile.am.  See the note there.
WITH_JITTER=$with_jitter
AC_SUBST([WITH_JITTER])

dnl We need to determine the endianness of the host system.  The
dnl following macro is also supposed to work when cross-compiling.

AC_C_BIGENDIAN

dnl Use libtextstyle if available.  Otherwise, use the dummy header
dnl file provided by gnulib's libtextstyle-optional module.

gl_LIBTEXTSTYLE_OPTIONAL
AX_LIBTEXTSTYLE_HYPERLINK

dnl The hyperlinks server is optional.  It is enabled by default if
dnl there is a libtextstyle with hyperlinks support available.

AC_ARG_ENABLE([hserver],
              AS_HELP_STRING([--enable-hserver],
                             [Enable the terminal hyperlinks server (default is YES)]),
              [hserver_enabled=$enableval], [hserver_enabled=$has_hyper_support])

if test "x$has_hyper_support" = "xno"; then
  hserver_enabled=no
fi

AM_CONDITIONAL([HSERVER], [test "x$hserver_enabled" = "xyes"])
if test "x$hserver_enabled" = "xyes"; then
  AC_DEFINE([HAVE_HSERVER], 1,
            [Defined if the hyperlinks server is available])
fi

dnl The GUI is optional, and depends on the availability of Tcl and
dnl Tk.

POKE_TCLTK

AC_ARG_ENABLE([gui],
              AS_HELP_STRING([--enable-gui],
                             [Enable the GUI (default is YES)]),
              [gui_enabled=$enableval], [gui_enabled=$has_tcltk])

AM_CONDITIONAL([GUI], [test "x$gui_enabled" = "xyes"])

dnl Generate output files
AC_CONFIG_FILES(Makefile
                gl/Makefile
                gl-libpoke/Makefile
                gl-gui/Makefile
                libpoke/Makefile
                poke/Makefile
                gui/Makefile
                pickles/Makefile
                maps/Makefile
                doc/Makefile
                man/Makefile
                po/Makefile.in
                etc/Makefile
                testsuite/Makefile
                testsuite/poke.libpoke/Makefile
                testsuite/poke.mi-json/Makefile)
AC_CONFIG_FILES([run],
                [chmod +x,-w run])
AC_OUTPUT

dnl Report warnings

if test "x$HAVE_LIBTEXTSTYLE" = "xno"; then
   AC_MSG_WARN([libtextstyle was not found in the system. Poke's output won't be styled.])
fi

if test "x$hserver_enabled" = "xno"; then
   AC_MSG_WARN([building poke without terminal hyperlinks server support.
     Install a hyperlinks-capable libtextstyle and use --enable-hserver to activate it.])
fi

if test "x$libnbd_enabled" != "xyes"; then
   AC_MSG_WARN([building poke without NBD io space support.
     Install libnbd to use it.])
fi

if test "x$mi_enabled" = "xno"; then
   AC_MSG_WARN([building poke without the machine interface support.
     Install libjson-c and use --enable-mi to activate it.])
fi

if test "x$gui_enabled" = "xno"; then
   AC_MSG_WARN([building poke without GUI support.
     Install tcl-dev and tk-dev and use --enable-gui to activate it.])
fi

dnl Report errors

if test "x$have_gc" = "xno"; then
   AC_MSG_ERROR([can't find the Boehm GC library.  Please install it.])
fi

if test "x$gl_cv_lib_readline" = "xno"; then
   AC_MSG_ERROR([can't find an usable libreadline.  Please install one.])
fi

dnl End of configure.ac
