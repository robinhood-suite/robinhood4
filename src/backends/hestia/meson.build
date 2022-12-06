# This file is part of the RobinHood Library
# Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

cpp = meson.get_compiler('cpp')
libhestia = cpp.find_library('hestia_lib', disabler: true, required: false)

librbh_hestia_glue = shared_library(
    'rbh-hestia-glue',
    sources: [
        'hestia_glue.cpp',
    ],
    dependencies: [libhestia],
    install: true,
)

librbh_hestia = library(
    'rbh-hestia',
    sources: [
        'hestia.c',
        'plugin.c',
    ],
    version: librbh_hestia_version, # defined in include/robinhood/backends
    link_with: [librobinhood, librbh_hestia_glue],
    include_directories: rbh_include,
    install: true,
)
