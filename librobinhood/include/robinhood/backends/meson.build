# This file is part of RobinHood 4
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

# Posix backend

librbh_posix_conf = configuration_data()

librbh_posix_conf.set('RBH_POSIX_BACKEND_MAJOR', 1)
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

librbh_mongo_conf.set('RBH_MONGO_BACKEND_MAJOR', 1)
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

if get_option('lustre').enabled()
    librbh_lustre_conf = configuration_data()

    librbh_lustre_conf.set('RBH_LUSTRE_BACKEND_MAJOR', 1)
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
endif

# Hestia backend

if get_option('hestia').enabled()
    librbh_hestia_conf = configuration_data()

    librbh_hestia_conf.set('RBH_HESTIA_BACKEND_MAJOR', 0)
    librbh_hestia_conf.set('RBH_HESTIA_BACKEND_MINOR', 0)
    librbh_hestia_conf.set('RBH_HESTIA_BACKEND_RELEASE', 0)

    librbh_hestia_version = '@0@.@1@.@2@'.format(
        librbh_hestia_conf.get('RBH_HESTIA_BACKEND_MAJOR'),
        librbh_hestia_conf.get('RBH_HESTIA_BACKEND_MINOR'),
        librbh_hestia_conf.get('RBH_HESTIA_BACKEND_RELEASE')
    )

    librbh_hestia_h = configure_file(input: 'hestia.h.in', output: 'hestia.h',
                                     configuration: librbh_hestia_conf)

    install_headers(librbh_hestia_h, subdir: 'robinhood/backends')
endif

librbh_retention_conf = configuration_data()

librbh_retention_conf.set('RBH_RETENTION_PLUGIN_MAJOR', 0)
librbh_retention_conf.set('RBH_RETENTION_PLUGIN_MINOR', 0)
librbh_retention_conf.set('RBH_RETENTION_PLUGIN_RELEASE', 0)

librbh_retention_version = '@0@.@1@.@2@'.format(
    librbh_retention_conf.get('RBH_RETENTION_PLUGIN_MAJOR'),
    librbh_retention_conf.get('RBH_RETENTION_PLUGIN_MINOR'),
    librbh_retention_conf.get('RBH_RETENTION_PLUGIN_RELEASE')
)

librbh_retention_h = configure_file(input: 'retention.h.in',
                                    output: 'retention.h',
                                    configuration: librbh_retention_conf)

install_headers(librbh_retention_h, subdir: 'robinhood/backends')

if not build_mfu
    subdir_done()
endif

# Posix MPIFileUtils extension plugin

librbh_mfu_conf = configuration_data()

librbh_mfu_conf.set('RBH_MFU_PLUGIN_MAJOR', 1)
librbh_mfu_conf.set('RBH_MFU_PLUGIN_MINOR', 0)
librbh_mfu_conf.set('RBH_MFU_PLUGIN_RELEASE', 0)

librbh_mfu_version = '@0@.@1@.@2@'.format(
    librbh_mfu_conf.get('RBH_MFU_PLUGIN_MAJOR'),
    librbh_mfu_conf.get('RBH_MFU_PLUGIN_MINOR'),
    librbh_mfu_conf.get('RBH_MFU_PLUGIN_RELEASE')
)

librbh_mfu_h = configure_file(input: 'mfu.h.in',
                                    output: 'mfu.h',
                                    configuration: librbh_mfu_conf)

install_headers(librbh_mfu_h, subdir: 'robinhood/backends')

# MPI-FILE backend

librbh_mpi_file_conf = configuration_data()

librbh_mpi_file_conf.set('RBH_MPI_FILE_BACKEND_MAJOR', 0)
librbh_mpi_file_conf.set('RBH_MPI_FILE_BACKEND_MINOR', 0)
librbh_mpi_file_conf.set('RBH_MPI_FILE_BACKEND_RELEASE', 0)

librbh_mpi_file_version = '@0@.@1@.@2@'.format(
    librbh_mpi_file_conf.get('RBH_MPI_FILE_BACKEND_MAJOR'),
    librbh_mpi_file_conf.get('RBH_MPI_FILE_BACKEND_MINOR'),
    librbh_mpi_file_conf.get('RBH_MPI_FILE_BACKEND_RELEASE')
)

librbh_mpi_file_h = configure_file(input: 'mpi_file.h.in',
                                   output: 'mpi_file.h',
                                   configuration: librbh_mpi_file_conf)

install_headers(librbh_mpi_file_h, subdir: 'robinhood/backends')
