# This file is part of RobinHood 4
# Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
# 		      alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

cc = meson.get_compiler('c')
liblustre = dependency('lustre', disabler: true, required: false)
if not liblustre.found()
    liblustre = cc.find_library('lustreapi', disabler:true, required: false)
endif

# Configuration checks
conf_data = configuration_data()

## Functions
have_fid_parse = cc.has_function('llapi_fid_parse', args: '-D_GNU_SOURCE',
		                         dependencies: [liblustre],
			                     prefix: '#include <lustre/lustreapi.h>')
conf_data.set('HAVE_FID_PARSE', have_fid_parse)

configure_file(input: 'config.h.in', output: 'config.h',
	       configuration: conf_data)

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
	dependencies: [librobinhood_dep, librbhfind_dep, liblustre],
	include_directories: include_dirs,
	install: true,
    c_args: '-DHAVE_CONFIG_H',
)
subdir('tests')
