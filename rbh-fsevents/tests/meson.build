# This file is part of the rbh-fsevents
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

liblustre = dependency('lustre', required: false)
if liblustre.found()
    subdir('lustre')
endif

libhestia = cc.find_library('hestia', dirs: '/usr/lib/hestia', required: false)
if libhestia.found()
    subdir('hestia')
endif
