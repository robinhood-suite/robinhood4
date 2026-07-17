# This file is part of RobinHood 4
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#               alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

parallel_integration_tests = [
  'test_sync_logs',
  'test_find_logs',
]

if test_lustre
    parallel_integration_tests += ['test_fsevents_logs']
endif

foreach t: parallel_integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-log', is_parallel: true)
endforeach
