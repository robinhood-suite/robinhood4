# This file is part of rbh-sync
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

project(
	'rbh-sync',
	'c',
	version: '0.0.0',
	license: 'LGPL3.0-or-later',
	default_options: [
		'warning_level=2',
		'werror=true',
	],
)

# GNU extensions
add_project_arguments(['-D_GNU_SOURCE',], language: 'c')

# Configuration checks
conf_data = configuration_data()

## Functions
cc = meson.get_compiler('c')
have_statx = cc.has_function('statx', args: '-D_GNU_SOURCE',
			     prefix: '#include <sys/stat.h>')
conf_data.set('HAVE_STATX', have_statx)

configure_file(input: 'config.h.in', output: 'config.h',
	       configuration: conf_data)
add_project_arguments(['-DHAVE_CONFIG_H',], language: 'c')

librobinhood = dependency('robinhood', version: '>=0.0.0')

executable(
	'rbh-sync',
	sources: [
		'rbh-sync.c',
		],
	dependencies: [librobinhood],
	install: true,
)
