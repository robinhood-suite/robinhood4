# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

python = import('python').find_installation()

install_subdir('lib',
               install_dir: python.get_install_dir()
)

install_data('rbh-update-retention.py',
             rename: 'rbh-ur',
             install_dir: get_option('bindir'),
             install_mode: ['rwxr-xr-x', 0, 0]
)

subdir('tests')
