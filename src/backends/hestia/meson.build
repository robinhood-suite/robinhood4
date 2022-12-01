# This file is part of the RobinHood Library
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
    include_directories: rbh_include,
    install: true,
)
