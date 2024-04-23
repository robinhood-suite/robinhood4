# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = ['test_compat', 'test_hsm_state', 'test_fid',
                     'test_ost_index']

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e)
endforeach

non_parallel_integration_tests = ['test_stripe_count', 'test_stripe_size',
                                  'test_layout_pattern']
foreach t: non_parallel_integration_tests
    e = find_program(t + '.bash')
    test(t, e, is_parallel: false)
endforeach
