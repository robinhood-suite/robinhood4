# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

subdir('posix')

if test_mfu
    subdir('mpifile')
endif

if get_option('lustre').enabled()
    subdir('lustre')
endif
