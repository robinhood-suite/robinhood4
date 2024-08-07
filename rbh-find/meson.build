# This file is part of RobinHood 4
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
# 		      alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

project(
	'rbh-find',
	'c',
	version: '1.0.0',
	license: 'LGPL3.0-or-later',
	default_options: [
		'warning_level=3',
		'werror=true',
	],
)

add_project_arguments('-Wno-pedantic', language : 'c')

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

librobinhood = dependency('robinhood', version: '>=1.0.0')
miniyaml = dependency('miniyaml', version: '>=0.1.0')

# "." is necessary for config.h
include_dirs = include_directories('.', 'include')

executable(
	'rbh-find',
	sources: [
		'rbh-find.c',
		'src/actions.c',
		'src/core.c',
		'src/filters.c',
		'src/find_cb.c',
		'src/parser.c',
		'src/utils.c',
		],
	dependencies: [librobinhood, miniyaml],
	include_directories: include_dirs,
	install: true,
)

# Build rbh-find library for rbh-find overloads
rbhfind_include = include_directories('.', 'include')
subdir('include')
subdir('src')

pkg_mod = import('pkgconfig')
pkg_mod.generate(
        libraries: librbhfind,
        version: meson.project_version(),
        name: 'librbh-find',
        filebase: 'rbh-find',
        description: 'C-API to librobinhood find-like mechanism'
)
subdir('tests')
