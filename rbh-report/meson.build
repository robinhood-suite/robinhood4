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
        'src/columns.c',
        'src/parser/group_by.c',
        'src/parser/output.c',
        'src/printer/common_print.c',
        'src/printer/csv_print.c',
        'src/printer/pretty_print.c',
    ],
    dependencies: [librobinhood_dep, librbhfilters_dep],
    include_directories: include_dir,
    install: true,
)

subdir('tests')
