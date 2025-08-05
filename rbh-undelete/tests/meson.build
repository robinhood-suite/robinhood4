# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#               alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = []
non_parallel_integration_tests = []

if liblustre.found()
    non_parallel_integration_tests += ['test_list', 'test_undelete']
endif

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-undelete')
endforeach

foreach t: non_parallel_integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-undelete',
         is_parallel: false)
endforeach
