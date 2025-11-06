# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

subdir('posix')
subdir('db')

if test_lustre
    subdir('lustre')
endif

if test_mfu
    subdir('mpifile')
endif

if libhestia.found()
    subdir('hestia')
endif

if libs3.found()
    subdir('s3')
endif
