# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later


libpcre = dependency('libpcre', required: true, disabler: false)

library('pcre',
        sources: ['pcre.c'],
        dependencies: [sqlite3, libpcre],
        name_prefix: '',
        install: true,
        install_dir: get_option('libdir') / 'sqlite3')
