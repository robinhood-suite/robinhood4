# SPDX-License-Identifer: LGPL-3.0-or-later
project(
    'robinhood4',
    'c',
    version: '0.1.0',
    license: 'LGPL3.0-or-later',
    default_options: [
        'warning_level=2',
        'werror=true',
    ],
)

specfile = configuration_data()
specfile.set('version', meson.project_version())
configure_file(
    input: 'robinhood4.ocean.spec.in',
    output: 'robinhood4.ocean.spec',
    configuration: specfile,
)
meson.add_dist_script('utils/add_specfile_to_dist.sh')
custom_target('rpm',
    output: 'robinhood4-' + meson.project_version() + '.ocean.rpm',
    command: [
        '../utils/make_rpm.sh', meson.current_build_dir(), meson.project_version()
    ],
)
