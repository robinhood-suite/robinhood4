# This file is part of Robinhood 4
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

project(
    'rbh-sync',
    'c',
    version: '1.0.0',
    license: 'LGPL3.0-or-later',
    default_options: [
        'warning_level=2',
        'werror=true',
    ],
)

# GNU extensions
add_project_arguments(['-D_GNU_SOURCE',], language: 'c')

# Dependencies
librobinhood = dependency('robinhood', version: '>=1.0.0')
miniyaml = dependency('miniyaml', version: '>=0.1.0')

executable(
    'rbh-sync',
    sources: [
        'rbh-sync.c',
    ],
    dependencies: [librobinhood, miniyaml],
    install: true,
)

subdir('tests')
