# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
# 		      alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later
#
# author: Dominique Martinet <dominique.martinet@cea.fr>
# author: Quentin Bouget <quentin.bouget@cea.fr>

librobinhood = library(
	'robinhood',
	sources: [
		'backend.c',
		'filter.c',
		'fsentry.c',
		'fsevent.c',
		'itertools.c',
		'plugin.c',
		'plugins/backend.c',
		'queue.c',
		'ring.c',
		'sstack.c',
		'stack.c',
		'uri.c',
	],
	version: '0.0.0',
	link_args: ['-ldl'],
	include_directories: rbh_include,
	install: true,
)

librbh_posix = library(
	'rbh-posix',
	sources: [
		'backends/posix.c',
		'plugins/backends/posix.c',
		],
	version: librbh_posix_version, # defined in include/robinhood/backends
	link_with: librobinhood,
	include_directories: rbh_include,
	install: true,
)

libmongoc = dependency('libmongoc-1.0', version: '>=1.3.6')
libbson = dependency('libbson-1.0', version: '>=1.3.5')

librbh_mongo = library(
	'rbh-mongo',
	sources: [
		'backends/mongo.c',
		'plugins/backends/mongo.c',
		],
	# defined in include/robinhood/backends
	version: librbh_mongo_version,
	link_with: librobinhood,
	dependencies: [libmongoc, libbson],
	include_directories: rbh_include,
	install: true,
)
