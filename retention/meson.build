# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = ['test_update_retention', 'test_retention']

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'retention',
         is_parallel: false)
endforeach

install_data(
    'rbh_update_retention',
    install_dir: get_option('bindir'),
    rename: 'rbh-update-retention'
)

install_data(
    'rbh_retention',
    install_dir: get_option('bindir'),
    rename: 'rbh-retention'
)
