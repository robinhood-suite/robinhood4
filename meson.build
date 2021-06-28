# SPDX-License-Identifer: LGPL-3.0-or-later

project(
    'rbh-fsevents',
    'c',
    version: '0.0.0',
    license: 'LGPL3.0-or-later',
    default_options: [
        'warning_level=2',
        'werror=true',
    ],
)

# GNU extensions
add_project_arguments(['-D_DEFAULT_SOURCE', '-D_GNU_SOURCE',], language: 'c')

# Configuration checks
conf_data = configuration_data()

## Functions
cc = meson.get_compiler('c')
have_statx = cc.has_function('statx', args: '-D_GNU_SOURCE',
                             prefix: '#include <sys/stat.h>')
conf_data.set('HAVE_STATX', have_statx)

configure_file(input: 'config.h.in', output: 'config.h',
               configuration: conf_data)
add_project_arguments(['-DHAVE_CONFIG_H',], language: 'c')

# Dependencies
librobinhood = dependency('robinhood', version: '>=0.0.0')
libyaml = dependency('yaml-0.1', version: '>=0.2.5')
miniyaml = dependency('miniyaml', version: '>=0.0.0')

includes = include_directories('.', 'include')

executable(
    'rbh-fsevents',
    sources: [
        'rbh-fsevents.c',
        'src/serialization.c',
        'src/sources/file.c',
        'src/sinks/backend.c',
        'src/sinks/file.c',
    ],
    include_directories: includes,
    dependencies: [librobinhood, libyaml, miniyaml],
    install: true,
)
