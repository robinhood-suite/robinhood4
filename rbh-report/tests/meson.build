# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = [
  'test_alias',
  'test_avg',
  'test_count',
  'test_group_by',
  'test_max',
  'test_min',
  'test_multi_output',
  'test_output_format',
  'test_parsing',
  'test_range',
  'test_sum',
]

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-report', timeout: 120)
endforeach
