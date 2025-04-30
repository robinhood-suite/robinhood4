# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

librbhfilters = library(
    'rbh-filters',
    sources: [
        'core.c',
    ],
    version: meson.project_version(),
    dependencies: [librobinhood_dep],
    include_directories: include_dirs,
    install: true,
)

librbhfilters_dep = declare_dependency(
    link_with: librbhfilters,
    include_directories: include_dirs
)
