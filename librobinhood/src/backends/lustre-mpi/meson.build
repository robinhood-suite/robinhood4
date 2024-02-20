# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

cc = meson.get_compiler('c')

liblustre = dependency('lustre', disabler: true, required: false)
if not liblustre.found()
        liblustre = cc.find_library('lustreapi', disabler:true, required: false)
endif

libmfu = cc.find_library('mfu', disabler: true, required: false)
libmpi = dependency('ompi', disabler: true, required: false)

librbh_lustre_mpi = library(
    'rbh-lustre-mpi',
    sources: [
        'lustre-mpi.c',
    ],
    version: librbh_lustre_version, # defined in include/robinhood/backends
    link_with: [librobinhood, librbh_posix],
    dependencies:[
        liblustre,
        libmfu,
        libmpi
    ],
    include_directories: rbh_include,
    install: true,
)

