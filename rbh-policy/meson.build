# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

python = import('python').find_installation()

configure_file(
  input: 'rbh-policy.py',
  output: 'rbh-policy',
  install: true,
  install_dir: get_option('bindir'),
  copy: true,
  install_mode: ['rwxr-xr-x', 0, 0]
)

