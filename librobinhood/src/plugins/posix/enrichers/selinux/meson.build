# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

librbh_posix_selinux_ext = library(
    'rbh-posix-selinux-ext',
    sources: [
	'plugin.c',
	'selinux.c',
	'range_parser.c',
    'parser.c',
    'filter.c',
    ],
    version: librbh_selinux_version, # defined in include/robinhood/backends
    link_with: librobinhood,
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
