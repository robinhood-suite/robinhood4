# This file is part of RobinHood 4
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = [
    'test_posix_mongo_sync', 'test_mongo_mongo_sync', 'test_config'
]

if liblustre.found()
    integration_tests += ['test_lustre_mongo_sync']
endif

if libhestia.found()
    integration_tests += ['test_hestia_mongo_sync']
endif

if libs3.found()
    integration_tests += ['test_s3_mongo_sync']
endif

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, is_parallel: false, env: backend_path_env,
         suite: 'rbh-sync', timeout: 120)
endforeach

non_parallel_integration_tests = ['test_alias']
foreach t: non_parallel_integration_tests
    e = find_program(t + '.bash')
    test(t, e, is_parallel: false, env: backend_path_env,
         suite: 'rbh-sync')
endforeach

# Tests with MPI
r = run_command('../../utils/check_mpirun.sh', check: false)

if r.returncode() == 0 and libmfu.found()
    integration_tests = ['test_mongo_mpifile_sync']
    foreach t: integration_tests
        e = find_program(t + '.bash')
        test(t, e, is_parallel: false, env: backend_path_env, suite: 'rbh-sync',
             timeout: 120)
    endforeach

    backend_path_env_for_mpi_tests.set('WITH_MPI', 'true')
    integration_tests_mpi = ['test_posix_mongo_sync', 'test_mpifile_mongo_sync']

    if liblustre.found()
        integration_tests_mpi += ['test_lustre_mongo_sync']
    endif

    foreach t: integration_tests_mpi
        e = find_program(t + '.bash')
        test(t + '_mpi', e, is_parallel: false, timeout: 120,
             env: backend_path_env_for_mpi_tests, suite: 'rbh-sync')
    endforeach

endif
