# This file is part of Robinhood 4
# Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

librbh_hestia = library(
    'rbh-hestia',
    sources: [
        'hestia.c',
        'plugin.c',
    ],
    version: librbh_hestia_version, # defined in include/robinhood/backends
    link_with: [librobinhood],
    dependencies: [libhestia],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
