# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = [
  'test_avg',
  'test_count',
  'test_filter',
  'test_group_by',
  'test_helper',
  'test_max',
  'test_min',
  'test_multi_output',
  'test_output_format',
  'test_parsing',
  'test_range',
  'test_sum',
  'test_verbose',
]

if liblustre.found()
    integration_tests += ['test_lustre_filter']
endif

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-report', timeout: 120)
endforeach

test(
     'test_alias',
     find_program('test_alias.bash'),
     env: backend_path_env,
     suite: 'rbh-report',
     timeout: 120,
     is_parallel: false
)
