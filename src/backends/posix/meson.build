# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
# 		      alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later
#
# author: Dominique Martinet <dominique.martinet@cea.fr>
# author: Quentin Bouget <quentin.bouget@cea.fr>

librbh_posix = library(
	'rbh-posix',
	sources: [
		'posix.c',
		'plugin.c',
		],
	version: librbh_posix_version, # defined in include/robinhood/backends
	link_with: librobinhood,
	include_directories: rbh_include,
	install: true,
)
