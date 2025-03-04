# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = ['test_compat', 'test_hsm_state', 'test_fid',
                     'test_ost_index', 'test_expiration', 'test_printf',
                     'test_pool', 'test_comp_start', 'test_mdt_index']

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-find-lustre')
endforeach

non_parallel_integration_tests = ['test_stripe_count', 'test_stripe_size',
                                  'test_layout_pattern']
foreach t: non_parallel_integration_tests
    e = find_program(t + '.bash')
    test(t, e, is_parallel: false, env: backend_path_env,
         suite: 'rbh-find-lustre')
endforeach
