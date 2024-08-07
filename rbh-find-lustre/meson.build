# This file is part of RobinHood 4
# Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
# 		      alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

project(
	'rbh-find-lustre',
	'c',
	version: '1.0.0',
	license: 'LGPL3.0-or-later',
	default_options: [
		'warning_level=2',
		'werror=true',
	],
)

librobinhood = dependency('robinhood', version: '>=1.0.0')
miniyaml = dependency('miniyaml', version: '>=0.1.0')
librbhfind = dependency('rbh-find', version: '>=1.0.0')

cc = meson.get_compiler('c')
liblustre = dependency('lustre', disabler: true, required: false)
if not liblustre.found()
    liblustre = cc.find_library('lustreapi', disabler:true, required: false)
endif

# Project extentions
add_project_arguments('-D_GNU_SOURCE', language: 'c')

# Configuration checks
conf_data = configuration_data()

## Functions
have_fid_parse = cc.has_function('llapi_fid_parse', args: '-D_GNU_SOURCE',
		                         dependencies: [liblustre],
			                     prefix: '#include <lustre/lustreapi.h>')
conf_data.set('HAVE_FID_PARSE', have_fid_parse)

configure_file(input: 'config.h.in', output: 'config.h',
	       configuration: conf_data)
add_project_arguments(['-DHAVE_CONFIG_H',], language: 'c')

# "." is necessary for config.h
include_dirs = include_directories('.', 'include')

executable(
	'rbh-lfind',
	sources: [
		'rbh-find-lustre.c',
		'src/filters.c',
		'src/parser.c',
		'src/actions.c',
		],
	dependencies: [librobinhood, miniyaml, librbhfind, liblustre],
	include_directories: include_dirs,
	install: true,
)
subdir('tests')
