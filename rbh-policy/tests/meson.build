# This file is part of RobinHood
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

build_lib_path = meson.build_root() / 'librobinhood' / 'src'
tests_src = meson.source_root() / 'rbh-policy' / 'tests'
src_root = meson.source_root() / 'rbh-policy'

backend_path_env.set('PYTHONPATH', src_root)
backend_path_env.set('BUILD_LIB_PATH', build_lib_path)

python_tests = [
  'test_models_conditions',
  'test_models_fileclass',
  'test_cpython',
  'test_config',
  'test_actions',
  'test_utils'
]
foreach t : python_tests
  test(
    t,
    python,
    args: [tests_src / (t + '.py')],
    suite: 'rbh-policy',
    env: backend_path_env,
    workdir: src_root
  )
endforeach
