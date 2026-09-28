# This file is part of RobinHood 4
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#               alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

parallel_integration_tests = [
  'test_delete',
  'test_find_stats',
  'test_gc_stats',
  'test_report_stats',
  'test_sync_stats',
  'test_time',
]

foreach t: parallel_integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-stats', is_parallel: true)
endforeach

if test_lustre
    non_parallel_integration_tests = ['test_all_stats',
                                      'test_format',
                                      'test_fsevents_stats',
                                      'test_stats_count']

    foreach t: non_parallel_integration_tests
        e = find_program(t + '.bash')
        test(t, e, env: backend_path_env, suite: 'rbh-stats', is_parallel: false)
    endforeach
endif
