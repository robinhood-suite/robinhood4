# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

librbh_mpi_file = library(
    'rbh-mpi-file',
    sources: [
        'action.c',
        'filter.c',
        'mpi_file.c',
        'parser.c',
        'plugin.c',
        'plugin_callback_common.c',
        'pred.c',
        '../iter_mpi/iter_mpi.c',
    ],
    version: librbh_mpi_file_version, # defined in include/robinhood/backends
    link_with: [librobinhood, librbh_posix],
    dependencies: [
        libmfu,
        libmpi
    ],
    include_directories: rbh_include,
    install: true,
)

