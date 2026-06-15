# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_selinux = false
getenforce_prog = find_program('getenforce', required: false)
chcon_prog = find_program('chcon', required: false)

if getenforce_prog.found() and chcon_prog.found()
    getenforce_res = run_command(getenforce_prog, check: false)

    if getenforce_res.stdout().strip() != 'Disabled'
        test_selinux = true
    endif
endif

librbh_posix_selinux_ext = library(
    'rbh-posix-selinux-ext',
    sources: [
        'plugin.c',
        'selinux.c',
        'range_parser.c',
    ],
    version: librbh_selinux_version, # defined in include/robinhood/backends
    link_with: librobinhood,
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
