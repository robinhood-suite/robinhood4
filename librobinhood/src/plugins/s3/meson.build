# This file is part of Robinhood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

s3_wrapper = library(
    's3_wrapper',
    sources: [
        's3_wrapper.h',
        's3_wrapper.cpp',
    ],
    dependencies: [libs3],
    install: true,
)

librbh_s3 = library(
    'rbh-s3',
    sources: [
        's3.c',
        'plugin.c',
    ],
    version: librbh_s3_version, # defined in include/robinhood/backends
    link_with: [librobinhood, s3_wrapper],
    dependencies: [],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
