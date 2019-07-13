# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
# 		      alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later
#
# author: Quentin Bouget <quentin.bouget@cea.fr>

include $(top_srcdir)/template.mk.am

lib_LTLIBRARIES = librobinhood.la
librobinhood_la_SOURCES = ../src/backend.c \
			  ../src/filter.c \
			  ../src/fsentry.c \
			  ../src/fsevent.c \
			  ../src/itertools.c \
			  ../src/uri.c

lib_LTLIBRARIES += librbh-posix.la
librbh_posix_la_SOURCES = ../src/backends/posix.c
librbh_posix_la_LIBADD = librobinhood.la
