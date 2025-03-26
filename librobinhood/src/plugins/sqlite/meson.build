# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

sqlite3 = dependency('sqlite3', version: '>=1.3.6')
jansson = dependency('jansson', version: '>=1.16.0')

librbh_sqlite = library(
    'rbh-sqlite',
    sources: [
        'backend.c',
        'db_cursor.c',
        'filter.c',
        'json/encode.c',
        'plugin.c',
        'schema.c',
        'update.c',
    ],
    version: librbh_sqlite_version, # defined in include/robinhood/backends
    dependencies: [ sqlite3, jansson, librobinhood_dep ],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
