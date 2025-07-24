# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

librbh_s3_mpi_ext = library(
    'rbh-s3-mpi-ext',
    sources: [
        'iterator.c',
        'plugin.c',
    ],
    version: librbh_s3_mpi_version, # defined in include/robinhood/backends
    link_with: [librobinhood, librbh_s3, s3_wrapper],
    dependencies: [
        libmpi
    ],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
