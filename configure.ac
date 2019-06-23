# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
# 		      alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later
#
# author: Quentin Bouget <quentin.bouget@cea.fr>

AC_PREREQ([2.69])
AC_INIT([librobinhood], [0.0.1], [quentin.bouget@cea.fr])
AC_CONFIG_SRCDIR([include/robinhood.h])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AC_USE_SYSTEM_EXTENSIONS

AM_INIT_AUTOMAKE([foreign subdir-objects])

# --disable-assert option
AC_HEADER_ASSERT

# libtool
LT_INIT

# Checks for programs.
AC_PROG_CC_C99
if test "$ac_cv_prog_cc_c99" == "no"; then
    AC_MSG_ERROR([cannot set the compiler to accept C99])
fi
AC_PROG_CC_STDC # Use the most up-to-date standard

# Checks for libraries.
PKG_CHECK_MODULES([CHECK], [check >= 0.9.9])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_FUNCS([statx])

AC_CONFIG_FILES([Makefile
                 include/Makefile
                 lib/Makefile
                 tests/Makefile
                 tests/unit/Makefile])
AC_OUTPUT
