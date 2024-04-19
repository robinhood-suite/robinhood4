# This file is part of RobinHood 4
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

# Configuration checks
conf_data = configuration_data()

## Functions
cc = meson.get_compiler('c')
have_statx = cc.has_function('statx', args: '-D_GNU_SOURCE',
                             prefix: '#include <sys/stat.h>')
conf_data.set('HAVE_STATX', have_statx)

configure_file(input: 'config.h.in', output: 'config.h',
               configuration: conf_data)

# "." is necessary for config.h
rbhfind_include = include_directories('.', 'include')

subdir('include')
subdir('src')

executable(
    'rbh-find',
    sources: [
        'rbh-find.c',
    ],
    dependencies: [librobinhood_dep, librbhfind_dep],
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)

pkg_mod = import('pkgconfig')
pkg_mod.generate(
        libraries: librbhfind,
        version: meson.project_version(),
        name: 'librbh-find',
        filebase: 'rbh-find',
        description: 'C-API to librobinhood find-like mechanism'
)
subdir('tests')
