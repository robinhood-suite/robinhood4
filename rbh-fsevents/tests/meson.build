# This file is part of RobinHood 4
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

subdir('unit')

integration_tests = []

liblustre = dependency('lustre', required: false)
if liblustre.found()
    subdir('lustre')
endif

libhestia = dependency('hestia', required: false)
if libhestia.found()
    subdir('hestia')
endif
