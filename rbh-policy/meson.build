# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

python = import('python').find_installation('python3.9')

install_subdir('rbhpolicy',
  install_dir: python.get_install_dir()
)

install_data('rbh-policy.py',
  rename: 'rbh-policy',
  install_dir: get_option('bindir'),
  install_mode: ['rwxr-xr-x', 0, 0]
)

subdir('tests')
