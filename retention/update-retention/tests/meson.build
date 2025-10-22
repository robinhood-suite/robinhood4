# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

python = import('python').find_installation()

tests_src = meson.source_root()/'retention'/'update-retention'/'tests'
src_root = meson.source_root()/'retention'/'update-retention'

python_tests = [
  'test_update_retention'
]

backend_path_env.set('PYTHONPATH', src_root)

foreach t : python_tests
  test(
    t,
    python,
    args: [tests_src / (t + '.py')],
    suite: 'retention',
    env: backend_path_env,
    workdir: src_root
  )
endforeach
