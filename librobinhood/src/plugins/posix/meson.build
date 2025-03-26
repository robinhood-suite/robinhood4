# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

librbh_posix = library(
    'rbh-posix',
    sources: [
        'action.c',
        'extension.c',
        'filter.c',
        'fts_iter.c',
        'plugin.c',
        'parser.c',
        'posix.c',
        'xattrs_mapping.c'
    ],
    version: librbh_posix_version, # defined in include/robinhood/backends
    link_with: librobinhood,
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)

librbh_posix_dep = declare_dependency(
    link_with: librbh_posix,
    include_directories: rbh_include
)

if build_mfu
  subdir('iterators/mfu')
endif

subdir('enrichers/lustre')
subdir('enrichers/retention')
