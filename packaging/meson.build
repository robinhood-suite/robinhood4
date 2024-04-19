specfile = configuration_data()
specfile.set('version', meson.project_version())
configure_file(
    input: 'robinhood4.iosea.spec.in',
    output: 'robinhood4.iosea.spec',
    configuration: specfile,
)
meson.add_dist_script('utils/add_specfile_to_dist.sh')
custom_target('rpm',
    output: 'robinhood4-' + meson.project_version() + '.rpm',
    command: [
        '../utils/make_rpm.sh', meson.current_build_dir(), meson.project_version()
    ],
)

