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
  'test_db',
]

foreach t: non_parallel_integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-info', is_parallel: false)
endforeach

foreach t: parallel_integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-info', is_parallel: true)
endforeach

if test_mfu
    foreach t: ['test_mpi_file']
        e = find_program(t + '.bash')
        test(t, e, env: backend_path_env_for_mpi_tests,
             suite: ['rbh-info', 'mpi'])
    endforeach
endif

if test_lustre
    foreach t: ['test_lustre']
        e = find_program(t + '.bash')
        test(t, e, env: backend_path_env,
             suite: ['rbh-info', 'lustre'])
    endforeach
endif

if libs3.found()
    foreach t: ['test_s3']
        e = find_program(t + '.bash')
        test(t, e, env: backend_path_env,
             suite: ['rbh-info', 's3'])
    endforeach
endif
