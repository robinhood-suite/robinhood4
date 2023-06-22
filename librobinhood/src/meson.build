# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

libdl = cc.find_library('dl', required: false)

librobinhood = library(
    'robinhood',
    sources: [
        'backend.c',
        'filter.c',
        'fsentry.c',
        'fsevent.c',
        'id.c',
        'itertools.c',
        'lu_fid.c',
        'plugin.c',
        'plugins/backend.c',
        'queue.c',
        'ring.c',
        'ringr.c',
        'sstack.c',
        'stack.c',
        'statx.c',
        'uri.c',
        'utils/uri.c',
        'value.c',
    ],
    version: meson.project_version(),
    dependencies: [ libdl ],
    include_directories: rbh_include,
    install: true,
)

subdir('backends')
