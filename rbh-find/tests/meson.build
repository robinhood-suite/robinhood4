# This file is part of RobinHood 4
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = [
  'test_actions',
  'test_perm',
  'test_printf',
  'test_size',
  'test_time',
  'test_xattr',
]

cc = meson.get_compiler('c')
libmfu = cc.find_library('mfu', disabler: true, required: false)

if libmfu.found()
    integration_tests += ['test_mpifile']
endif

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-find', timeout: 120)
endforeach
