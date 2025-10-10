# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later
#
include_dir = include_directories('.', 'include')

executable(
    'rbh-undelete',
    sources: [
        'rbh-undelete.c',
        'src/list.c',
        'src/path.c',
        'src/undelete.c'
    ],
    dependencies: [librobinhood_dep],
    include_directories: include_dir,
    install: true,
)

subdir('tests')
