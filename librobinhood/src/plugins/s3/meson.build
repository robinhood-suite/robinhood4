# This file is part of RobinHood
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_s3 = false

if get_option('s3').disabled()
    subdir_done()
endif

libs3 = dependency('aws-cpp-sdk-s3', disabler: true, required: true)

if is_disabler(libs3)
    subdir_done()
endif

test_s3 = true

s3_wrapper = library(
    'rbh-s3_wrapper',
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
        'action.c',
        's3.c',
        'config.c',
        'filter.c',
        'parser.c',
        'plugin.c',
        'plugin_callback_common.c',
        'utils.c',
    ],
    version: librbh_s3_version, # defined in include/robinhood/backends
    link_with: [librobinhood, s3_wrapper],
    dependencies: [],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)

subdir('iterators/mpi')
