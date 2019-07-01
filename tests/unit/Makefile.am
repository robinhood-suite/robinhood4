# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
# 		      alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later
#
# author: Quentin Bouget <quentin.bouget@cea.fr>

include $(top_srcdir)/template.mk.am

AM_CFLAGS += @CHECK_CFLAGS@
AM_LDFLAGS += @CHECK_LIBS@
LDADD = $(top_builddir)/lib/librobinhood.la

check_PROGRAMS = check_backend check_filter check_fsentry check_itertools
TESTS = $(check_PROGRAMS)
