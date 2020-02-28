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
		'id.c',
		'itertools.c',
		'lu_fid.c',
		'plugin.c',
		'plugins/backend.c',
		'queue.c',
		'ring.c',
		'sstack.c',
		'stack.c',
		'uri.c',
		'utils/uri.c',
		'value.c',
	],
	version: libversion,
	link_args: ['-ldl'],
	include_directories: rbh_include,
	install: true,
)

subdir('backends')
