# This file is part of RobinHood 4
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = ['test_posix_mongo_sync', 'test_mongo_mongo_sync']

liblustre = dependency('lustre', disabler: true, required: false)

libhestia = dependency('hestia', disabler: true, required: false)

if liblustre.found()
    integration_tests += ['test_lustre_mongo_sync']
endif

if libhestia.found()
    integration_tests += ['test_hestia_mongo_sync']
endif

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, is_parallel: false)
endforeach

# Tests with MPI
cc = meson.get_compiler('c')
libmfu = cc.find_library('mfu', disabler: true, required: false)
r = run_command('../../utils/check_mpirun.sh', check: false)

if r.returncode() == 0 and libmfu.found()
    integration_tests = ['test_mongo_mpifile_sync', 'test_mpifile_mongo_sync']
    foreach t: integration_tests
        e = find_program(t + '.bash')
        test(t, e, is_parallel: false)
    endforeach

    env = environment()
    env.prepend('WITH_MPI', 'true')
    integration_tests_mpi = ['test_posix_mongo_sync']

    if liblustre.found()
        integration_tests_mpi += ['test_lustre_mongo_sync']
    endif

    foreach t: integration_tests_mpi
        e = find_program(t + '.bash')
        test(t + '_mpi', e, is_parallel: false, env: env, timeout: 60)
    endforeach

endif
