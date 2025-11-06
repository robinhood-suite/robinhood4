# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = ['test_db_db_sync']

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, is_parallel: false, env: backend_path_env,
         suite: 'rbh-sync', timeout: 120)
endforeach
