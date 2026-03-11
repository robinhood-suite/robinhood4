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

install_subdir('rbhpolicy',
  install_dir: python.get_install_dir()
)

install_data('rbh-policy.py',
  rename: 'rbh-policy',
  install_dir: get_option('bindir'),
  install_mode: ['rwxr-xr-x', 0, 0]
)

subdir('tests')
