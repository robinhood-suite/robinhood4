# This file is part of the RobinHood project
# Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

librbhfind = library(
    'rbh-find',
    sources: [
        'actions.c',
        'filters.c',
        'parser.c',
        'utils.c',
    ],
    version: meson.project_version(),
    dependencies: [librobinhood],
    include_directories: rbhfind_include,
    install: true,
)
