# SPDX-License-Identifer: LGPL-3.0-or-later

# Configuration checks
conf_data = configuration_data()

## Functions
cc = meson.get_compiler('c')
have_statx = cc.has_function('statx', args: '-D_GNU_SOURCE',
                             prefix: '#include <sys/stat.h>')
conf_data.set('HAVE_STATX', have_statx)

configure_file(input: 'config.h.in', output: 'config.h',
               configuration: conf_data)

# Dependencies
extra_sources = []
extra_dependencies = []
extra_args = []

if liblustre.found()
    extra_sources += [
        'src/sources/lustre.c',
        'src/enrichers/posix/lustre.c',
    ]
    extra_args += ['-DHAVE_LUSTRE']
endif

if libhestia.found()
    extra_dependencies += [ libhestia ]
endif

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
        'src/enrichers/posix/posix.c',
        'src/sources/yaml_file.c',
        'src/sources/file.c',
        'src/sources/hestia.c',
        'src/sources/utils.c',
        'src/sinks/backend.c',
        'src/sinks/file.c',
    ] + extra_sources,
    include_directories: includes,
    dependencies: [
        librobinhood_dep, miniyaml, liblustre,
    ] + extra_dependencies,
    c_args: ['-DHAVE_CONFIG_H'] + extra_args,
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
    dependencies: [librobinhood_dep, miniyaml, liblustre],
    install: true,
    c_args: ['-DHAVE_CONFIG_H'] + extra_args,
)

subdir('tests')
