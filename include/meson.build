# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
# 		      alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later
#
# author: Dominique Martinet <dominique.martinet@cea.fr>
# author: Quentin Bouget <quentin.bouget@cea.fr>

install_headers('robinhood.h')

install_headers('robinhood/backend.h',
		'robinhood/filter.h',
		'robinhood/fsentry.h',
		'robinhood/fsevent.h',
		'robinhood/iterator.h',
		'robinhood/itertools.h',
		'robinhood/plugin.h',
		'robinhood/statx.h',
		'robinhood/stack.h',
		'robinhood/uri.h',
		subdir: 'robinhood')

install_headers('robinhood/plugins/backend.h',
		subdir: 'robinhood/plugins')

subdir('robinhood/backends')
