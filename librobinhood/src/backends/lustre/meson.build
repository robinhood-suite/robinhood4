# This file is part of RobinHood 4
# Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

if get_option('lustre').disabled()
    librbh_lustre = disabler()
    subdir_done()
endif

# TODO remove once lustre is no longer a backend
librbh_lustre = library(
    'rbh-lustre',
    sources: [
        'lustre.c',
        'plugin.c',
    ],
    version: librbh_lustre_version, # defined in include/robinhood/backends
    link_with: [librobinhood, librbh_posix],
    dependencies: [liblustre],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)

librbh_posix_lustre_ext = library(
    'rbh-posix-lustre-ext',
    sources: [
        'lustre.c',
        'plugin.c',
    ],
    version: librbh_lustre_version, # defined in include/robinhood/backends
    link_with: [librobinhood, librbh_posix],
    dependencies: [liblustre],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
