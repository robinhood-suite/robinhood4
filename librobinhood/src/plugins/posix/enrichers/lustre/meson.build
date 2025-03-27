# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

if get_option('lustre').disabled()
    librbh_lustre = disabler()
    subdir_done()
endif

lustre_conf_data = configuration_data()

lustre_conf_data.merge_from(conf_data)

have_fid_parse = cc.has_function('llapi_fid_parse', args: '-D_GNU_SOURCE',
                                 dependencies: [liblustre],
                                 prefix: '#include <lustre/lustreapi.h>')
lustre_conf_data.set('HAVE_FID_PARSE', have_fid_parse)

configure_file(input: 'config.h.in', output: 'config.h',
               configuration: lustre_conf_data)

librbh_posix_lustre_ext = library(
    'rbh-posix-lustre-ext',
    sources: [
        'action.c',
        'filter.c',
        'lustre.c',
        'parser.c',
        'plugin.c',
    ],
    version: librbh_lustre_version, # defined in include/robinhood/backends
    link_with: [librobinhood, librbh_posix],
    dependencies: [liblustre],
    include_directories: rbh_include,
    install: true,
    c_args: '-DHAVE_CONFIG_H',
)
