# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

libext2fs = cc.find_library('libext2fs', required: false, disabler: true)
libcom_err = cc.find_library('com_err', required: false, disabler: true)
glib2 = dependency('glib-2.0', required: false, disabler: true)

ldiskfs_conf_data = configuration_data()

ldiskfs_conf_data.merge_from(conf_data)

# Here is the definition of the 'ext2fs_xattrs_iterate' function from the Lustre
# version:
#
# errcode_t ext2fs_xattrs_iterate(struct ext2_xattr_handle *h,
#                                int (*func)(char *name, char *value,
#                                            size_t value_len,
#                                            ext2_ino_t inode_num, void *data),
#                                void *data);
#
# The kernel version is missing the 'ext2_ino_t' parameter in the anonymous
# function, so we have to check which one to use depending on the e2fsprogs
# version installed.
#
# XXX: Trying to compile a piece of code to check for either version doesn't
# work for some reason, so we instead compile an almost-empty header containing
# only the ext2fs include in order to get its content, then search the
# 'ext2fs_xattrs_iterate' function with grep to determine the version to use.

result = run_command('gcc', '-E', 'compil.h', '-o', 'tmp_file')
result = run_command('grep', '-A', '4', 'ext2fs_xattrs_iterate', 'tmp_file')
result = result.stdout().contains('ext2_ino_t')

message('ldiskfs uses lustre\'s e2fsprogs "ext2fs_xattrs_iterate" function: ' +
        result.to_string())

ldiskfs_conf_data.set('LUSTRE_VERSION_E2FSPROGS_XATTRS_ITERATE',
                      result)

configure_file(input: 'config.h.in', output: 'config.h',
               configuration: ldiskfs_conf_data)

librbh_ldiskfs = library(
    'rbh-ldiskfs',
    sources: [
        'backend.c',
        'dcache.c',
        'filter.c',
        'plugin.c',
        'scan_target.c',
        'xattrs.c',
    ],
    version: librbh_ldiskfs_version, # defined in include/robinhood/backends
    dependencies: [ librobinhood_dep, libext2fs, glib2, libcom_err ],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
