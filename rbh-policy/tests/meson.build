# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

python3 = import('python').find_installation('python3.9')

tests_src = meson.source_root() / 'rbh-policy' / 'tests'
src_root = meson.source_root() / 'rbh-policy'

python_tests = [
  'test_generate_sources.py'
]

foreach t : python_tests
  test_name = t.replace('.py', '')

  test(
    test_name,
    python3,
    args: [tests_src / t],
    suite: 'rbh-policy',
    env: ['PYTHONPATH=' + src_root],
    workdir: src_root
  )
endforeach
