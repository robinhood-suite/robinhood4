# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#            alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

python = import('python').find_installation('python3.9')

configure_file(
  input: 'rbh-policy.py',
  output: 'rbh-policy',
  install: true,
  install_dir: get_option('bindir'),
  copy: true,
  install_mode: ['rwxr-xr-x', 0, 0]
)

generate_sources_py = configure_file(
  input: 'generate_sources.py',
  output: 'generate_sources.py',
  install: false,
  copy: true,
)

install_dir = python.get_install_dir()
gen = find_program('generate_sources.py')

out = run_command(gen).stdout().strip()
lines = out == '' ? [] : out.split('\n')

foreach l : lines
  parts = l.split(':')
  subdir = parts[0]
  path = parts[1]

  python.install_sources(
    files(path),
    subdir: subdir,
    install_dir: install_dir
  )
endforeach

subdir('tests')
