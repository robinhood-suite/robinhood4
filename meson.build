# SPDX-License-Identifer: LGPL-3.0-or-later
project(
    'robinhood4',
    'c',
    version: '1.0.0',
    license: 'LGPL3.0-or-later',
    default_options: [
        'warning_level=2',
        'werror=true',
    ],
)

specfile = configuration_data()
specfile.set('version', meson.project_version())
specfile.set('file', 'robinhood4.ocean.spec')
configure_file(
    input: 'robinhood4.ocean.spec.in',
    output: specfile.get('file'),
    configuration: specfile,
)
meson.add_dist_script('utils/add_specfile_to_dist.sh', specfile.get('file'))
custom_target('rpm',
    output: 'robinhood4-' + meson.project_version() + '.ocean.rpm',
    command: [
        # The `get_option` isn't generic at all, but it will suffice for now
        '../utils/make_rpm.sh', meson.current_build_dir(), meson.project_version(), get_option('build_mpi').to_string()
    ],
)
