# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

if not check.found()
    subdir_done()
endif

foreach t: ['check_backend', 'check_filter', 'check_fsentry',
            'check_fsevent', 'check_hashmap', 'check_id', 'check_itertools',
            'check_list', 'check_lu_fid', 'check_plugin', 'check_queue',
            'check_ring', 'check_ringr', 'check_sstack', 'check_stack',
            'check_statx', 'check_uri', 'check_utils', 'check_value']
    test(t,
         executable(t, t + '.c',
                    dependencies: [check, miniyaml],
                    link_with: [librobinhood],
                    include_directories: rbh_include,
                    c_args: '-DHAVE_CONFIG_H'),
         env: backend_path_env,
         suite: 'librobinhood')
endforeach

foreach t: ['check_posix']
    test(t,
         executable(t, t + '.c',
                    dependencies: [check, miniyaml],
                    link_with: [librobinhood, librbh_posix],
                    include_directories: rbh_include,
                    c_args: '-DHAVE_CONFIG_H'),
         env: backend_path_env,
         suite: 'librobinhood')
endforeach

foreach t: ['check_retention']
    test(t,
         executable(t, t + '.c',
                    dependencies: [check],
                    link_with: [
                        librobinhood,
                        librbh_posix,
                        librbh_posix_retention_ext
                    ],
                    include_directories: rbh_include,
                    c_args: '-DHAVE_CONFIG_H'),
         env: backend_path_env,
         suite: 'librobinhood')
endforeach

if liblustre.found()
    foreach t: ['check_lustre']
        test(t,
             executable(t, t + '.c',
                        dependencies: [check, miniyaml],
                        link_with: [librobinhood, librbh_posix],
                        include_directories: rbh_include,
                        c_args: '-DHAVE_CONFIG_H'),
            env: backend_path_env,
            suite: 'librobinhood')
    endforeach
endif

bash = find_program('bash', disabler: true, required: false)

if test_mfu
    backend_path_env.prepend('CK_FORK', 'no')
    backend_path_env.prepend('RBH_CONFIG_PATH',
        meson.current_source_dir() + '../../../utils/tests/test_config.yaml')
    builddir = meson.current_build_dir()

    foreach t: ['check_mfu']
    test(t,
         executable(t, t + '.c',
                    dependencies: [check, miniyaml],
                    link_with: [
                        librobinhood,
                        librbh_posix,
                    ],
                    include_directories: rbh_include,
                    c_args: '-DHAVE_CONFIG_H'),
         env: backend_path_env,
         suite: 'librobinhood')
    endforeach

    foreach t: ['check_posix_mpi']
        executable(t, t + '.c',
            dependencies: [check, miniyaml],
            link_with: [librobinhood, librbh_posix],
            include_directories: rbh_include,
            c_args: '-DHAVE_CONFIG_H')
        test(t, bash,
             args: [files(t + '.sh'), builddir],
             depends: [librbh_posix],
             env: backend_path_env,
             suite: 'librobinhood')
    endforeach

    if liblustre.found()
        foreach t: ['check_lustre_mpi']
            executable(t, t + '.c',
                dependencies: [check, miniyaml],
                link_with: [librobinhood, librbh_posix],
                include_directories: rbh_include,
                c_args: '-DHAVE_CONFIG_H')
            test(t, bash,
                 args: [files(t + '.sh'), builddir],
                depends: [librbh_posix],
                env: backend_path_env,
                suite: 'librobinhood')
        endforeach
    endif
endif
