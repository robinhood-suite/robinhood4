# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

if get_option('lustre').disabled()
    librbh_lustre_mpi = disabler()
    subdir_done()
endif

librbh_lustre_mpi = library(
    'rbh-lustre-mpi',
    sources: [
        'lustre_mpi.c',
        '../iter_mpi/iter_mpi.c',
        'plugin.c',
    ],
    version: librbh_lustre_mpi_version, # defined in include/robinhood/backends
    link_with: [librobinhood, librbh_posix, librbh_lustre],
    dependencies:[
        liblustre,
        libmfu,
        libmpi
    ],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
