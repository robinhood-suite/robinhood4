# This file is part of RobinHood 4
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

libmongoc = dependency('libmongoc-1.0', version: '>=1.3.6')
libbson = dependency('libbson-1.0', version: '>=1.16.0')
# This dependency will be removed soon
miniyaml = dependency('miniyaml', version: '>=0.0.0')

librbh_mongo = library(
    'rbh-mongo',
    sources: [
        'bson.c',
        'filter.c',
        'fields.c',
        'fsentry.c',
        'fsevent.c',
        'mongo.c',
        'options.c',
        'plugin.c',
        'value.c',
    ],
    version: librbh_mongo_version, # defined in include/robinhood/backends
    link_with: librobinhood,
    dependencies: [libmongoc, libbson, miniyaml],
    include_directories: rbh_include,
    install: true,
)
