# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

specfile = configuration_data()
specfile.set('version', meson.project_version())
configure_file(
    input: 'robinhood4.spec.in',
    output: 'robinhood4.spec',
    configuration: specfile,
)

build_options = configuration_data()
build_options.set('lustre', get_option('lustre').enabled())
build_options.set('mfu', get_option('mfu').enabled())
build_options.set('hestia', get_option('hestia').enabled())
build_options.set('version', meson.project_version())
configure_file(
    input: 'build_options.sh.in',
    output: 'build_options.sh',
    configuration: build_options
)

meson.add_dist_script('../utils/add_specfile_to_dist.sh')
custom_target('rpm',
    output: 'robinhood4-' + meson.project_version() + '.rpm',
    command: [ '../packaging/make_rpm.sh', meson.build_root() ],
)
