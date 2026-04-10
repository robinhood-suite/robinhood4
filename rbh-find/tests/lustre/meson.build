# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

integration_tests = [
    'test_comp',
    'test_composite',
    'test_extension_size',
    'test_fid',
    'test_hash_type',
    'test_hsm_state',
    'test_lustre_helper',
    'test_mdt_count',
    'test_mdt_index',
    'test_mirror',
    'test_ost_index',
    'test_pool',
    'test_project_id',
]

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: ['rbh-find', 'lustre'])
endforeach

non_parallel_integration_tests = [
    'test_layout_pattern',
    'test_stripe_count',
    'test_stripe_size',
]

foreach t: non_parallel_integration_tests
    e = find_program(t + '.bash')
    test(t, e, is_parallel: false, env: backend_path_env,
         suite: ['rbh-find', 'lustre'])
endforeach
