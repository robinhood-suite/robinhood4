# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

sphinx = find_program('sphinx-build', '/usr/bin/sphinx-build', required: true)

build = join_paths(meson.build_root(), 'doc/man/build')

r = run_command('mkdir', '-p', build)

if r.returncode() == 0 and sphinx.found()
    r = run_command(sphinx, 'source', build, '-b', 'man')
    if r.returncode() == 0
        message(r.stdout())

        g = run_command('find', build, '-name', '*.[1-9]')
        manpages = g.stdout().strip().split('\n')
        install_man(manpages)
    else
        message(r.stderr())
    endif
endif
