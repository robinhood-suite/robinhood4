# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

libext2fs = cc.find_library('libext2fs', disabler: true)
glib2 = dependency('glib-2.0', disabler: true)

librbh_ldiskfs = library(
    'rbh-ldiskfs',
    sources: [
        'backend.c',
        'dcache.c',
        'plugin.c',
        'scan_target.c',
    ],
    version: librbh_ldiskfs_version, # defined in include/robinhood/backends
    dependencies: [ librobinhood_dep, libext2fs, glib2 ],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
