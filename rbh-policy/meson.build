# This file is part of RobinHood
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

py_mod = import('python')
# This ugly for-loop is necessary because if a system has two versions of python
# installed, with one under 3.9 as the system's default and the other above,
# then the "python >= 3.9" check will fail, even though there is a version which
# passes the condition.
#
# However, we cannot directly request a specific python version which pass the
# check because if that version doesn't exist but another does, then the build
# will fail. This for-loop is the easiest solution we found unfortunately.
valid_versions = ['3.15', '3.14', '3.13', '3.12', '3.11', '3.10', '3.9']
foreach version: valid_versions
    python = py_mod.find_installation('python' + version, required: false)
    if python.found()
        break
    endif
endforeach

if not python.found()
    error('Python 3.9 or newer is required, couldn\'t find a good version')
endif

install_subdir('rbhpolicy',
  install_dir: python.get_install_dir()
)

install_data('rbh-policy.py',
  rename: 'rbh-policy',
  install_dir: get_option('bindir'),
  install_mode: ['rwxr-xr-x', 0, 0]
)

subdir('tests')
