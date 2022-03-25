# This file is part of the RobinHood Library
# Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

liblustre = dependency('lustre', disabler: true, required: false)

librbh_lustre = library(
    'rbh-lustre',
    sources: [
        'lustre.c',
        'plugin.c',
    ],
    version: librbh_lustre_version, # defined in include/robinhood/backends
    link_with: [librobinhood, librbh_posix],
    dependencies: [liblustre],
    include_directories: rbh_include,
    install: true,
)
