# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
# | |         alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

project(
|   'rbh-capabilities',
|   'c',
|   version: '0.0.0',
|   license: 'LGPL3.0-or-later',
|   default_options: [
|   |   'warning_level=2',
|   |   'werror=true',
|   ],
)

# GNU extensions
add_project_arguments(['-D_GNU_SOURCE',], language: 'c')
librobinhood = dependency('robinhood', version: '>=0.0.0')

executable(
|   'rbh-capabilities',
|   sources: [
|   |   'rbh-capabilities.c',
|   |   ],
|   dependencies: [librobinhood],
|   install: true,
)

subdir('tests')
