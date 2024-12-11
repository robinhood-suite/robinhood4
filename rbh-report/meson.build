# This file is part of Robinhood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

include_dir = include_directories('include')

executable(
    'rbh-report',
    sources: [
        'rbh-report.c',
        'src/group_by.c',
        'src/output.c',
        'src/print.c',
    ],
    dependencies: [librobinhood_dep],
    include_directories: include_dir,
    install: true,
)

subdir('tests')
