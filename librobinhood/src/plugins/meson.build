# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

backend_path_env = environment()

# This dual environment is necessary because the tests can be run with or
# without MPI, and that is determined by another environment variable. However,
# adding that variable to the normal environment defined above will modify the
# behaviour of all the tests, which is not intended. To circumvent this issue,
# we create another environment specific to the MPI tests, which can then have
# both the proper LD_LIBRARY_PATH and the MPI variable
backend_path_env_for_mpi_tests = environment()
ld_library_path = ''

# .. and also add paths for plugins required by tests that require one
library_dirs = [
    '..',
    'hestia',
    'mongo',
    'sqlite',
    'mpi_file',
    'posix',
    'posix/enrichers/lustre',
    'posix/enrichers/retention',
    'posix/iterators/mfu',
]
foreach backend: library_dirs
    # FIXME env.prepend only work with meson > 0.58.0
    # meson gives this warning otherwise:
    # Overriding previous value of environment variable 'LD_LIBRARY_PATH' with a
    # new one
    # XXX meson provides no way of retrieving an environment variable, so we
    # can't get the librobinhood library (which is located into
    # librobinhood/src) into the LD_LIBRARY_PATH except by adding .. here
    ld_library_path += ':' + meson.current_build_dir() + '/' + backend
endforeach

backend_path_env.set('LD_LIBRARY_PATH', ld_library_path)
backend_path_env_for_mpi_tests.set('LD_LIBRARY_PATH', ld_library_path)

subdir('mongo')
subdir('posix')
subdir('sqlite')

if get_option('hestia').enabled()
    subdir('hestia')
endif

if build_mfu
    subdir('mpi_file')
endif
