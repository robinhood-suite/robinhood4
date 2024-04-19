# This file is part of RobinHood 4
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

backend_path_env = environment()
ld_library_path = ''

# .. and also add paths for plugins required by tests that require one
foreach backend: ['posix', 'lustre', 'lustre_mpi', 'posix_mpi', 'mongo']
    # FIXME env.prepend does not seem to work
    # meson gives this warning:
    # Overriding previous value of environment variable 'LD_LIBRARY_PATH' with a
    # new one
    ld_library_path += ':' + meson.current_build_dir() + '/' + backend
endforeach

backend_path_env.set('LD_LIBRARY_PATH', ld_library_path)

subdir('mongo')
subdir('posix')
subdir('posix_mpi')
subdir('lustre')
subdir('lustre_mpi')
subdir('hestia')
subdir('mpi_file')
