# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_acl = false

getfacl_prog = find_program('getfacl', required: false)
setfacl_prog = find_program('setfacl', required: false)

test_acl = getfacl_prog.found() and setfacl_prog.found()

librbh_posix_acl_ext = library(
    'rbh-posix-acl-ext',
    sources: [
        'plugin.c',
        'acl_xattr.c',
        'acl.c',
        'filter.c',
        'parser.c',
    ],
    version: librbh_acl_version, # defined in include/robinhood/backends
    link_with: librobinhood,
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
