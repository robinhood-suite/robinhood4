# This file is part of RobinHood 4
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

jansson_dep = dependency('jansson', version: '>=1.16.0')

librbh_posix = library(
    'rbh-posix',
    sources: [
        'action.c',
        'config.c',
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
    dependencies: jansson_dep,
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

if build_lustre
    subdir('enrichers/lustre')
endif

subdir('enrichers/retention')
