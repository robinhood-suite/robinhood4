# This file is part of RobinHood
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_mpi_file = false

if get_option('mfu').disabled()
    subdir_done()
endif

libmfu = cc.find_library('mfu', dirs: '/usr/lib64/openmpi/lib',
                         disabler: true, required: false)
libmpi = dependency('ompi', disabler: true, required: false)

if not libmpi.found() and not libmfu.found()
    subdir_done()
endif

r = run_command(meson.project_source_root() + '/utils/check_mpirun.sh',
                check: false)
if r.returncode() == 0
    test_mpi_file = true
else
    message('"check_mpirun" failed, do not run MPI File Utils specific tests')
endif

librbh_mpi_file = library(
    'rbh-mpi-file',
    sources: [
        'action.c',
        'filter.c',
        'mpi_file.c',
        'parser.c',
        'plugin.c',
        'plugin_callback_common.c',
        'pred.c',
    ],
    version: librbh_mpi_file_version, # defined in include/robinhood/backends
    link_with: [librobinhood, librbh_posix],
    dependencies: [
        libmfu,
        librbh_posix_mfu_ext_dep,
        libmpi
    ],
    include_directories: rbh_include,
    install: true,
)
