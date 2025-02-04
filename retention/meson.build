# This file is part of RobinHood 4
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

if get_option('lustre').disabled()
    subdir_done()
endif

integration_tests = ['test_update_retention']

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env,
         suite: 'retention')
endforeach

install_data(
    'rbh_update_retention',
    install_dir: get_option('bindir'),
    rename: 'rbh-update-retention'
)
