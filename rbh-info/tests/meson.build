# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#               alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

non_parallel_integration_tests = [
  'info',
]

parallel_integration_tests = [
  'get_info',
  'test_posix',
  'test_mongo',
]

if liblustre.found()
    parallel_integration_tests += ['test_lustre']
endif

foreach t: non_parallel_integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-info', is_parallel: false)
endforeach

foreach t: parallel_integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-info', is_parallel: true)
endforeach

if test_mfu
    e = find_program('test_mpi_file.bash')
    test('test_mpi_file', e, env: backend_path_env_for_mpi_tests,
         suite: 'rbh-info')
endif
