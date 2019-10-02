# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
# 		      alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later
#
# author: Dominique Martinet <dominique.martinet@cea.fr>
# author: Quentin Bouget <quentin.bouget@cea.fr>

check = dependency('check', version: '>=0.9.9', required: false)

if check.found()
	foreach t: ['check_backend', 'check_filter', 'check_fsentry',
		    'check_fsevent', 'check_itertools', 'check_plugin',
		    'check_sstack', 'check_stack', 'check_uri']
		test(t, executable(t, t + '.c',
				    dependencies: [check],
				    link_with: [librobinhood],
				    include_directories: rbh_include))
	endforeach

	foreach t: ['check_posix']
		test(t, executable(t, t + '.c',
				   dependencies: [check],
				   link_with: [librobinhood, librbh_posix],
				   include_directories: rbh_include))
	endforeach
else
	message('check is missing: skipping unit testing')
endif
