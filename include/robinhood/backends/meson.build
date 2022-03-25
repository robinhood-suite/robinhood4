# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

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

# Mongo backend

librbh_mongo_conf = configuration_data()

librbh_mongo_conf.set('RBH_MONGO_BACKEND_MAJOR', 0)
librbh_mongo_conf.set('RBH_MONGO_BACKEND_MINOR', 0)
librbh_mongo_conf.set('RBH_MONGO_BACKEND_RELEASE', 0)

librbh_mongo_version = '@0@.@1@.@2@'.format(
    librbh_mongo_conf.get('RBH_MONGO_BACKEND_MAJOR'),
    librbh_mongo_conf.get('RBH_MONGO_BACKEND_MINOR'),
    librbh_mongo_conf.get('RBH_MONGO_BACKEND_RELEASE')
)

librbh_mongo_h = configure_file(input: 'mongo.h.in', output: 'mongo.h',
                                configuration: librbh_mongo_conf)

install_headers(librbh_mongo_h, subdir: 'robinhood/backends')

# Lustre backend

librbh_lustre_conf = configuration_data()

librbh_lustre_conf.set('RBH_LUSTRE_BACKEND_MAJOR', 0)
librbh_lustre_conf.set('RBH_LUSTRE_BACKEND_MINOR', 0)
librbh_lustre_conf.set('RBH_LUSTRE_BACKEND_RELEASE', 0)

librbh_lustre_version = '@0@.@1@.@2@'.format(
    librbh_lustre_conf.get('RBH_LUSTRE_BACKEND_MAJOR'),
    librbh_lustre_conf.get('RBH_LUSTRE_BACKEND_MINOR'),
    librbh_lustre_conf.get('RBH_LUSTRE_BACKEND_RELEASE')
)

librbh_lustre_h = configure_file(input: 'lustre.h.in', output: 'lustre.h',
                                 configuration: librbh_lustre_conf)

install_headers(librbh_lustre_h, subdir: 'robinhood/backends')
