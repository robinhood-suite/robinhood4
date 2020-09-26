# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

project(
    'librobinhood',
    'c',
    version: '0.0.0',
    license: 'LGPL3.0-or-later',
    default_options: [
        'werror=true',
        'c_std=c11',
    ],
)

# GNU extensions
cc = meson.get_compiler('c')
options = cc.get_supported_arguments([
    '-Wno-gnu-alignof-expression', # gcc >= 10.1.0 (maybe less)
])
add_project_arguments(['-D_GNU_SOURCE'] + options, language: 'c')

# Configuration checks
conf_data = configuration_data()

## Macro definitions
if not cc.compiles('#include <fts.h>', name: 'fts.h with _FILE_OFFSET_BITS=64')
    add_project_arguments(['-U_FILE_OFFSET_BITS'], language: 'c')
endif

## Functions
have_statx = cc.has_function('statx', args: '-D_GNU_SOURCE',
                             prefix: '#include <sys/stat.h>')
conf_data.set('HAVE_STATX', have_statx)

configure_file(input: 'config.h.in', output: 'config.h',
               configuration: conf_data)
add_project_arguments(['-DHAVE_CONFIG_H',], language: 'c')

# Recurse in subdirectories

# "." is necessary for config.h
rbh_include = include_directories('.', 'include')
subdir('include')
subdir('src')
subdir('tests/unit')

# Build a .pc file
pkg_mod = import('pkgconfig')
pkg_mod.generate(
    libraries: librobinhood,
    version: meson.project_version(),
    name: 'librobinhood',
    filebase: 'robinhood',
    description: 'C-API to mirror and query metadata'
)
