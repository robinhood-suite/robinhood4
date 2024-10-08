# This file is part of RobinHood 4
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

# Configuration checks
conf_data = configuration_data()

## Functions
have_statx = cc.has_function('statx', args: '-D_GNU_SOURCE',
                             prefix: '#include <sys/stat.h>')
conf_data.set('HAVE_STATX', have_statx)

## Members
have_stx_mnt_id = cc.has_member('struct statx', 'stx_mnt_id',
                                args: '-D_GNU_SOURCE',
                                prefix: '#include <sys/stat.h>')
conf_data.set('HAVE_STX_MNT_ID', have_stx_mnt_id)

## Symbols
have_statx_attr_mount_root = cc.has_header_symbol('sys/stat.h',
                                                  'STATX_ATTR_MOUNT_ROOT',
                                                  args: '-D_GNU_SOURCE')
conf_data.set('HAVE_STATX_ATTR_MOUNT_ROOT', have_statx_attr_mount_root)

have_statx_attr_verity = cc.has_header_symbol('sys/stat.h',
                                              'STATX_ATTR_VERITY',
                                              args: '-D_GNU_SOURCE')
conf_data.set('HAVE_STATX_ATTR_VERITY', have_statx_attr_verity)

have_statx_attr_dax = cc.has_header_symbol('sys/stat.h',
                                           'STATX_ATTR_DAX',
                                           args: '-D_GNU_SOURCE')
conf_data.set('HAVE_STATX_ATTR_DAX', have_statx_attr_dax)

have_lov_user_magic_sel = cc.has_header_symbol('lustre/lustre_user.h',
                                               'LOV_USER_MAGIC_SEL',
                                               args: '-D_GNU_SOURCE')
conf_data.set('HAVE_LOV_USER_MAGIC_SEL', have_lov_user_magic_sel)

have_lov_user_magic_foreign = cc.has_header_symbol('lustre/lustre_user.h',
                                                   'LOV_USER_MAGIC_FOREIGN',
                                                   args: '-D_GNU_SOURCE')
conf_data.set('HAVE_LOV_USER_MAGIC_FOREIGN', have_lov_user_magic_foreign)

have_lustre_file_handle = cc.has_member(
    'struct lustre_file_handle',
    'lfh_child',
    prefix: '#include <lustre/lustre_user.h>',
    args: '-D_GNU_SOURCE'
)
conf_data.set('HAVE_LUSTRE_FILE_HANDLE', have_lustre_file_handle)

configure_file(input: 'config.h.in', output: 'config.h',
               configuration: conf_data)

# Recurse in subdirectories

# "." is necessary for config.h
rbh_include = include_directories('.', 'include', 'include/robinhood')
subdir('include')
subdir('src')
subdir('tests/unit')

# Build a .pc file
pkg_mod = import('pkgconfig')
pkg_mod.generate(
    librobinhood,
    version: meson.project_version(),
    name: 'librobinhood',
    filebase: 'robinhood',
    description: 'C-API to mirror and query metadata'
)
