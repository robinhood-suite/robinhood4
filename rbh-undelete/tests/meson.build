# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#               alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

if liblustre.found()
    non_parallel_integration_tests = ['test_list', 'test_undelete']

    foreach t: non_parallel_integration_tests
        e = find_program(t + '.bash')
        test(t, e, env: backend_path_env, suite: ['rbh-undelete', 'lustre'],
             is_parallel: false)
    endforeach
endif

