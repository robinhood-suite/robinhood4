# This file is part of RobinHood
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

py_mod = import('python')
python = py_mod.find_installation('python3', required: true)

if not python.language_version().version_compare('>=3.9')
error('Python 3.9 or newer is required, but found Python '
      + python.language_version())
endif

build_lib_path = meson.build_root() / 'librobinhood' / 'src'
tests_src = meson.source_root() / 'rbh-policy' / 'tests'
src_root = meson.source_root() / 'rbh-policy'

python_tests = [
  'test_models_conditions',
  'test_models_fileclass',
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
      'BUILD_LIB_PATH=' + build_lib_path,
      'LD_LIBRARY_PATH=' + build_lib_path + ':' +
                           build_lib_path / 'plugins/posix'
    ],
    workdir: src_root
  )
endforeach
