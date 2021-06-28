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

# Dependencies
librobinhood = dependency('robinhood', version: '>=0.0.0')

executable(
    'rbh-fsevents',
    sources: [
        'rbh-fsevents.c',
    ],
    dependencies: [librobinhood],
    install: true,
)
