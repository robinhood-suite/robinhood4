# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

librbh_mpi_file = library(
    'rbh-mpi-file',
    sources: [
        'mpi_file.c',
        'filter.c',
        'pred.c',
        'plugin.c',
    ],
    version: librbh_mpi_file_version, # defined in include/robinhood/backends
    link_with: [librobinhood, librbh_posix],
    dependencies: [
        libmfu,
        librbh_posix_mfu_ext_dep,
        libmpi
    ],
    include_directories: rbh_include,
    install: true,
)

