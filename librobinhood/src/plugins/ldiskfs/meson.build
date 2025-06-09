# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

libext2fs = cc.find_library('libext2fs', required: false, disabler: true)
libcom_err = cc.find_library('com_err', required: false, disabler: true)
glib2 = dependency('glib-2.0', required: false, disabler: true)

librbh_ldiskfs = library(
    'rbh-ldiskfs',
    sources: [
        'backend.c',
        'dcache.c',
        'plugin.c',
        'scan_target.c',
    ],
    version: librbh_ldiskfs_version, # defined in include/robinhood/backends
    dependencies: [ librobinhood_dep, libext2fs, glib2, libcom_err ],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
