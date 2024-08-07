# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

check = dependency('check', version: '>=0.9.9', required: false)

if not check.found()
    message('check is missing: skipping unit testing')
    subdir_done()
endif

miniyaml = dependency('miniyaml', version: '>=0.1.0')

# force tests to use the newly built binaries over paths in
# LD_LIBRARY_PATH. The problem is that recent linkers will
# use RUNPATH when meson requests -rpath, and runpath has
# lower priority than LD_LIBRARY_PATH (contrary to rpath)
# See https://github.com/mesonbuild/meson/issues/1383
env = environment()
env.prepend('LD_LIBRARY_PATH', meson.build_root() + '/src')
# .. and also add paths for plugins required by tests that require one
env.prepend('LD_LIBRARY_PATH', meson.build_root() + '/src/backends/posix')
env.prepend('LD_LIBRARY_PATH', meson.build_root() + '/src/backends/lustre')
env.prepend('LD_LIBRARY_PATH', meson.build_root() + '/src/backends/lustre_mpi')
env.prepend('LD_LIBRARY_PATH', meson.build_root() + '/src/backends/posix_mpi')

foreach t: ['check_backend', 'check_filter', 'check_fsentry',
            'check_fsevent', 'check_hashmap', 'check_id', 'check_itertools',
            'check_list', 'check_lu_fid', 'check_plugin', 'check_queue',
            'check_ring', 'check_ringr', 'check_sstack', 'check_stack',
            'check_statx', 'check_uri', 'check_value']
    test(t,
         executable(t, t + '.c',
                    dependencies: [check, miniyaml],
                    link_with: [librobinhood],
                    include_directories: rbh_include),
         env: env)
endforeach

foreach t: ['check_posix']
    test(t,
         executable(t, t + '.c',
                    dependencies: [check, miniyaml],
                    link_with: [librobinhood, librbh_posix],
                    include_directories: rbh_include),
         env: env)
endforeach

foreach t: ['check_lustre']
    test(t,
         executable(t, t + '.c',
                    dependencies: [check, miniyaml],
                    link_with: [librobinhood, librbh_lustre],
                    include_directories: rbh_include),
         env: env)
endforeach


r = run_command('../../../utils/check_mpirun.sh', check: false)
bash = find_program('bash', disabler: true, required: false)

if r.returncode() == 0 and libmfu.found()
    env.prepend('CK_FORK', 'no')
    foreach t: ['check_lustre_mpi']
        executable(t, t + '.c',
            dependencies: [check, miniyaml],
            link_with: [librobinhood, librbh_lustre_mpi],
            include_directories: rbh_include)
    endforeach

    builddir = meson.current_build_dir()
    test('check_lustre_mpi', bash,
         args: ['../tests/unit/check_lustre_mpi.sh', builddir],
         env: env)

    foreach t: ['check_posix_mpi']
        executable(t, t + '.c',
            dependencies: [check, miniyaml],
            link_with: [librobinhood, librbh_posix_mpi],
            include_directories: rbh_include)
    endforeach

    test('check_posix_mpi', bash,
         args: ['../tests/unit/check_posix_mpi.sh', builddir],
         env: env)

endif
