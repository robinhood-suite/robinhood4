# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

integration_tests = [
    'test_acl',
]

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: ['rbh-find', 'acl'])
endforeach

non_parallel_integration_tests = ['test_acl_rwxable',]

foreach t: non_parallel_integration_tests
    e = find_program(t + '.bash')
    test(t, e, is_parallel: false, env: backend_path_env, suite: 'rbh-find')
endforeach
