# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

python = import('python').find_installation()

tests_src = meson.source_root()/'retention'/'update-retention'/'tests'
src_root = meson.source_root()/'retention'/'update-retention'

tests = [
  'test_update_retention'
]

backend_path_env.set('PYTHONPATH', src_root)

foreach t: tests
    e = find_program(t + '.bash')
    test(t, e, is_parallel: false, env: backend_path_env,
         suite: 'retention', timeout: 120)
endforeach
