# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
# 		      alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later
#
# author: Quentin Bouget <quentin.bouget@cea.fr>

# Posix backend

librbh_posix_conf = configuration_data()

librbh_posix_conf.set('RBH_POSIX_BACKEND_MAJOR', 0)
librbh_posix_conf.set('RBH_POSIX_BACKEND_MINOR', 0)
librbh_posix_conf.set('RBH_POSIX_BACKEND_RELEASE', 0)

librbh_posix_version = '@0@.@1@.@2@'.format(
	librbh_posix_conf.get('RBH_POSIX_BACKEND_MAJOR'),
	librbh_posix_conf.get('RBH_POSIX_BACKEND_MINOR'),
	librbh_posix_conf.get('RBH_POSIX_BACKEND_RELEASE')
)

librbh_posix_h = configure_file(input: 'posix.h.in', output: 'posix.h',
				configuration: librbh_posix_conf)

install_headers(librbh_posix_h, subdir: 'robinhood/backends')
