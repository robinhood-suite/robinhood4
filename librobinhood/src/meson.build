# This file is part of the RobinHood Library
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

libdl = cc.find_library('dl', required: false)

librobinhood = library(
    'robinhood',
    sources: [
        'alias.c',
        'backend.c',
        'config.c',
        'filter.c',
        'fsentry.c',
        'fsevent.c',
        'hashmap.c',
        'id.c',
        'itertools.c',
        'list.c',
        'lu_fid.c',
        'mpi_rc.c',
        'plugin.c',
        'plugins/backend.c',
        'queue.c',
        'ring.c',
        'ringr.c',
        'serialization.c',
        'sstack.c',
        'stack.c',
        'statx.c',
        'uri.c',
        'utils/path.c',
        'utils/uri.c',
        'utils/regex.c',
        'utils/utils.c',
        'utils/open.c',
        'value.c',
    ],
    version: meson.project_version(),
    dependencies: [ libdl, miniyaml ],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)

librobinhood_dep = declare_dependency(
    link_with: librobinhood,
    include_directories: rbh_include
)

subdir('backends')
