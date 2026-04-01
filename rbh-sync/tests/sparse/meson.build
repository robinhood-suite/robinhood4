# This file is part of RobinHood
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

integration_tests = ['test_sparse_db_sync']

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env,
         suite: ['rbh-sync', 'sparse'], timeout: 120)
endforeach

if test_mfu
    foreach t: integration_tests
        e = find_program(t + '.bash')
        test(t, e, timeout: 120, env: backend_path_env_for_mpi_tests,
             suite: ['rbh-sync', 'sparse', 'mpi'])
    endforeach

endif
