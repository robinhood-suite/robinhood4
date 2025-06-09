# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

librbh_ldiskfs = library(
    'rbh-ldiskfs',
    sources: [
        'backend.c',
        'plugin.c',
    ],
    version: librbh_ldiskfs_version, # defined in include/robinhood/backends
    dependencies: [ librobinhood_dep ],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
