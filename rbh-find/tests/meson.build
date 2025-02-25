# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = [
  'test_actions',
  'test_empty',
  'test_newer',
  'test_perm',
  'test_printf',
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

r = run_command('../../utils/check_mpirun.sh', check: false)

if r.returncode() == 0 and libmfu.found()
    e = find_program('test_mpifile.bash')
    test('test_mpifile', e, is_parallel: false,
         env: backend_path_env, suite: 'rbh-find', timeout: 120)
endif

non_parallel_integration_tests = ['test_alias']

foreach t: non_parallel_integration_tests
    e = find_program(t + '.bash')
    test(t, e, is_parallel: false, env: backend_path_env,
         suite: 'rbh-find')
endforeach
