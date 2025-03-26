# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

librbh_posix = library(
    'rbh-posix',
    sources: [
        'action.c',
        'filter.c',
        'posix.c',
        'plugin.c',
        'parser.c',
        'xattrs_mapping.c'
    ],
    version: librbh_posix_version, # defined in include/robinhood/backends
    link_with: librobinhood,
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
