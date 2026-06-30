# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

project(
    'robinhood4',
    'c', 'cpp',
    version: '1.5.2',
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
        '-Wall',
        '-Werror',
        '-O3',
    ] + options,
    language: 'c'
)

miniyaml = dependency('miniyaml', version: '>=0.0.0')

check = dependency('check', disabler: true, version: '>=0.9.9', required: false)
if not check.found()
    message('check is missing: skipping unit testing')
endif

subdir('librobinhood')
subdir('doc')
subdir('rbh-sync')
subdir('rbh-find')
subdir('rbh-fsevents')
subdir('rbh-policy')
subdir('rbh-report')
subdir('rbh-info')
subdir('rbh-update-path')
subdir('rbh-gc')
if get_option('lustre').enabled()
    subdir('rbh-undelete')
endif
subdir('rbh-log')
subdir('retention')
subdir('packaging')
