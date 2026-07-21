# This file is part of RobinHood
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

subdir('posix')
subdir('retention')
subdir('sparse')
subdir('mongo')

if test_mfu
    subdir('mpifile')
endif

if test_lustre
    subdir('lustre')
endif

if test_s3
    subdir('s3')
endif

if test_selinux
    subdir('selinux')
endif

if test_acl
    subdir('acl')
endif
