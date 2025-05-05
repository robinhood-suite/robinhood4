# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

# Configuration checks
conf_data = configuration_data()

## Functions
cc = meson.get_compiler('c')
have_statx = cc.has_function('statx', args: '-D_GNU_SOURCE',
                             prefix: '#include <sys/stat.h>')
conf_data.set('HAVE_STATX', have_statx)

configure_file(input: 'config.h.in', output: 'config.h',
               configuration: conf_data)

# "." is necessary for config.h
include_dirs = include_directories('.', 'include')

executable(
    'rbh-find',
    sources: [
        'rbh-find.c',
        'src/actions.c',
        'src/core.c',
        'src/filters.c',
        'src/parser.c',
    ],
    dependencies: [librobinhood_dep, librbhfilters_dep],
    include_directories: include_dirs,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)

subdir('tests')
