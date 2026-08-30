# This file is part of RobinHood
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

librbh_posix_fts_ext = library(
    'rbh-posix-fts-ext',
    sources: [
        'iterator.c',
        'plugin.c',
    ],
    version: librbh_fts_version, # defined in include/robinhood/backends
    link_with: [ librobinhood, librbh_posix ],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)

librbh_posix_fts_ext_dep = declare_dependency(
    link_with: librbh_posix_fts_ext,
    include_directories: rbh_include
)
