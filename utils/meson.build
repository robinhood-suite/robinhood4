# This file is part of RobinHood 4
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

project(
    'rbh-utils',
    'c',
    version: '1.0.0',
    license: 'LGPL3.0-or-later',
    default_options: [
        'warning_level=2',
        'werror=true',
    ],
)

integration_tests = ['test_update_retention']

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e)
endforeach

install_data(
    'rbh_update_retention',
    install_dir: get_option('bindir'),
    rename: 'rbh-update-retention'
)
