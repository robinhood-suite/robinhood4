# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later
project(
    'robinhood4',
    'c', 'cpp',
    version: '0.1.0',
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

## Macro definitions
if not cc.compiles('#include <fts.h>', name: 'fts.h with _FILE_OFFSET_BITS=64')
    add_project_arguments(['-U_FILE_OFFSET_BITS'], language: 'c')
endif


add_project_arguments([
        '-D_DEFAULT_SOURCE',
        '-D_GNU_SOURCE',
        '-Wno-pedantic',
    ] + options,
    language: 'c'
)

subdir('librobinhood')
subdir('rbh-sync')
subdir('rbh-find')
subdir('rbh-fsevents')
subdir('rbh-find-lustre')
subdir('rbh-capabilities')
subdir('utils')
