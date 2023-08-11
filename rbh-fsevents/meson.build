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
extra_sources = []
extra_dependencies = []

librobinhood = dependency('robinhood', version: '>=0.0.0')
miniyaml = dependency('miniyaml', version: '>=0.0.0')
liblustre = dependency('lustre', required: false)
if not liblustre.found()
    liblustre = cc.find_library('lustreapi', required: false)
endif
if liblustre.found()
    extra_sources += [
        'src/sources/lustre.c',
        'src/enrichers/lustre.c',
    ]
    add_project_arguments(['-DHAVE_LUSTRE',], language: 'c')
endif

libhestia = cc.find_library('hestia', dirs: '/usr/lib/hestia',
                            disabler: false, required: false)
if libhestia.found()
    libjansson = dependency('jansson', version: '>=2.5')
    extra_dependencies += [libjansson]
    extra_sources += [
        'src/enrichers/hestia.c',
    ]
    add_project_arguments(['-DHAVE_HESTIA',], language: 'c')
endif

includes = include_directories('.', 'include')

fsevents_lib = static_library(
    'rbh-fsevents',
    sources: [
        'src/deduplicator.c',
        'src/deduplicator/fsevent_pool.c',
        'src/deduplicator/hash.c',
        'src/deduplicator/rbh_fsevent_utils.c',
        'src/enricher.c',
        'src/enrichers/posix.c',
        'src/serialization.c',
        'src/sources/yaml_file.c',
        'src/sources/file.c',
        'src/sources/hestia.c',
        'src/sources/utils.c',
        'src/sinks/backend.c',
        'src/sinks/file.c',
    ] + extra_sources,
    include_directories: includes,
    dependencies: [
        librobinhood, miniyaml, liblustre, libhestia
    ] + extra_dependencies,
)
fsevents_dep = declare_dependency(
    link_with: fsevents_lib,
    include_directories: includes
)

executable(
    'rbh-fsevents',
    sources: 'rbh-fsevents.c',
    include_directories: includes,
    link_with: fsevents_lib,
    dependencies: [librobinhood, miniyaml, liblustre],
    install: true,
)

subdir('tests')
