# This file is part of Robinhood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

include_dirs = include_directories('include')

executable(
    'rbh-update-path',
    sources: [
        'rbh-update-path.c',
        'src/utils.c',
        'src/children.c',
        'src/entry.c',
    ],
    dependencies: [librobinhood_dep],
    include_directories: include_dirs,
    install: true,
)

subdir('tests')
