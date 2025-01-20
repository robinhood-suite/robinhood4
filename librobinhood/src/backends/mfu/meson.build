# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

librbh_posix_mfu_ext = library(
    'rbh-posix-mfu-ext',
    sources: [
        'iterator.c',
        'plugin.c',
        'utils.c',
    ],
    version: librbh_mfu_version, # defined in include/robinhood/backends
    link_with: [ librobinhood, librbh_posix ],
    dependencies: [
        libmfu,
        libmpi
    ],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
