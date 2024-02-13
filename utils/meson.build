# This file is part of RobinHood 4
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

project(
    'rbh-utils',
    'c',
    version: '0.0.0',
    license: 'LGPL3.0-or-later',
    default_options: [
        'warning_level=2',
        'werror=true',
    ],
)

sync = find_program('rbh-sync')
if not sync.found()
    message('sync is missing: skipping utils testing')
    subdir_done()
endif

lfind = find_program('rbh-lfind')
if not lfind.found()
    message('lfind is missing: skipping utils testing')
    subdir_done()
endif

fsevents = find_program('rbh-fsevents')
if not fsevents.found()
    message('fsevents is missing: skipping utils testing')
    subdir_done()
endif

integration_tests = ['test_update_retention']

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e)
endforeach
