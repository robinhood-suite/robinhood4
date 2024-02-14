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

libmfu = cc.find_library('mfu')
libmpi = dependency('ompi')

librbh_lustre_mpi = library(
    'rbh-lustre-mpi',
    sources: [
        'lustre-mpi.c',
    ],
    version: librbh_lustre_version, # defined in include/robinhood/backends
    link_with: [librobinhood],
    dependencies:[
        liblustre,
        libmfu,
        libmpi
    ],
    include_directories: rbh_include,
    install: true,
)

