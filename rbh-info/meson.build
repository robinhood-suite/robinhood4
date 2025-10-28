# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

include_dir = include_directories('.', 'include')

executable(
    'rbh-info',
    sources: [
        'rbh-info.c',
        'src/capabilities.c',
        'src/info.c',
        'src/list.c',
    ],
    dependencies: [librobinhood_dep],
    include_directories: include_dir,
    install: true,
)

subdir('tests')
