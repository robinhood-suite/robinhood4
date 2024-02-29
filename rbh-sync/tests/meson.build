# This file is part of RobinHood 4
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = ['test_posix_mongo_sync', 'test_mongo_mongo_sync']

liblustre = dependency('lustre', disabler: true, required: false)

libhestia = dependency('hestia', disabler: true, required: false)

cc = meson.get_compiler('c')
libmfu = cc.find_library('mfu', disabler: true, required: false)

if libhestia.found()
    integration_tests += ['test_hestia_mongo_sync']
endif

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, is_parallel: false)
endforeach

if liblustre.found()
    env = environment()
    e = find_program('test_lustre_mongo_sync.bash')

    test('test_lustre_mongo_sync', e,
         is_parallel: false)

    r = run_command('../../utils/check_mpirun.sh')

    if r.returncode() == 0 and libmfu.found()
        env.prepend('WITH_MPI', 'true')
        test('test_lustre_mpi_mongo_sync', e,
             is_parallel: false,
             env: env)
    endif
endif
