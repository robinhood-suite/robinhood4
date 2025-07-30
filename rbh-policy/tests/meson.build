# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

python3 = import('python').find_installation('python3.9')

build_lib_path = meson.build_root() / 'librobinhood' / 'src'
tests_src = meson.source_root() / 'rbh-policy' / 'tests'
src_root = meson.source_root() / 'rbh-policy'

python_tests = [
  'test_installation',
  'test_cpython',
  'test_config',
  'test_utils'
]

foreach t : python_tests
  test(
    t,
    python3,
    args: [tests_src / (t + '.py')],
    suite: 'rbh-policy',
    env: [
      'PYTHONPATH=' + src_root,
      'BUILD_LIB_PATH=' + build_lib_path
    ],
    workdir: src_root
  )
endforeach
