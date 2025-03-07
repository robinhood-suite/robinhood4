# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

project(
    'robinhood4',
    'c', 'cpp',
    version: '1.2.0',
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

miniyaml = dependency('miniyaml', version: '>=0.0.0')

if get_option('lustre').enabled()
    liblustre = dependency('lustre', disabler: true, required: false)
    if not liblustre.found()
            liblustre = cc.find_library('lustreapi', disabler:true,
                                        required: false)
    endif
else
    liblustre = disabler()
endif

if get_option('mfu').enabled()
    libmfu = cc.find_library('mfu', disabler: true, required: false)
    libmpi = dependency('ompi', disabler: true, required: false)
else
    libmfu = disabler()
    libmpi = disabler()
endif

if get_option('hestia').enabled()
    libhestia = dependency('hestia', disabler: true, required: false)
else
    libhestia = disabler()
endif

check = dependency('check', disabler: true, version: '>=0.9.9', required: false)
if not check.found()
    message('check is missing: skipping unit testing')
endif

subdir('librobinhood')
subdir('rbh-sync')
subdir('rbh-find')
subdir('rbh-fsevents')
subdir('rbh-report')
if get_option('lustre').enabled()
    subdir('rbh-find-lustre')
endif
subdir('rbh-capabilities')
subdir('retention')
subdir('packaging')
