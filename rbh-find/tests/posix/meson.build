# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = [
  'test_actions',
  'test_blocks',
  'test_empty',
  'test_inum',
  'test_links',
  'test_newer',
  'test_perm',
  'test_posix_printf',
  'test_regex',
  'test_size',
  'test_time',
  'test_uid_gid',
  'test_xattr',
]

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-find', timeout: 120)
endforeach

non_parallel_integration_tests = ['test_posix_alias']

foreach t: non_parallel_integration_tests
    e = find_program(t + '.bash')
    test(t, e, is_parallel: false, env: backend_path_env, suite: 'rbh-find')
endforeach
