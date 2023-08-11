# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

check = dependency('check', version: '>=0.9.9', required: false)

if not check.found()
    message('check is missing: skipping unit testing')
    subdir_done()
endif

test_utils = library(
    'test_utils',
    sources: 'utils/utils.c',
    dependencies: [ librobinhood, fsevents_dep, check ],
    include_directories: 'utils'
)
test_utils_dep = declare_dependency(
    link_with: test_utils,
    include_directories: 'utils'
)

unit_tests = [
    'check_dedup'
]

foreach t: unit_tests
    e = executable(t, t + '.c',
                   dependencies: [
                       check,
                       test_utils_dep,
                       fsevents_dep,
                   ])

    test(t, e)
endforeach
