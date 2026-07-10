# This file is part of RobinHood 4
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

include_dir = include_directories('.', 'include')

executable(
    'rbh-log',
    sources: [
        'rbh-log.c',
        'src/sync.c',
    ],
    dependencies: [librobinhood_dep],
    include_directories: include_dir,
    install: true,
)

subdir('tests')
