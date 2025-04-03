# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

librbh_posix_retention_ext = library(
    'rbh-posix-retention-ext',
    sources: [
        'action.c',
        'plugin.c',
        'retention.c',
    ],
    version: librbh_retention_version, # defined in include/robinhood/backends
    link_with: librobinhood,
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
