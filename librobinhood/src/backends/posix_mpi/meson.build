# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

cc = meson.get_compiler('c')

libmfu = cc.find_library('mfu', disabler: true, required: false)
libmpi = dependency('ompi', disabler: true, required: false)

librbh_posix_mpi = library(
    'rbh-posix-mpi',
    sources: [
        'posix_mpi.c',
        '../iter_mpi/iter_mpi.c',
        'plugin.c',
    ],
    version: librbh_posix_mpi_version, # defined in include/robinhood/backends
    link_with: [librobinhood, librbh_posix],
    dependencies: [
        libmfu,
        libmpi
    ],
    include_directories: rbh_include,
    install: true,
)
