# This file is part of the RobinHood Library
# Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

check = dependency('check', version: '>=0.9.9', required: false)

if not check.found()
    message('check is missing: skipping unit testing')
    subdir_done()
endif

foreach t: ['check_backend', 'check_filter', 'check_fsentry',
            'check_fsevent', 'check_id', 'check_itertools',
            'check_lu_fid', 'check_queue', 'check_ring', 'check_ringr',
            'check_sstack', 'check_stack', 'check_uri', 'check_value']
    test(t, executable(t, t + '.c',
                       dependencies: [check],
                       link_with: [librobinhood],
                       include_directories: rbh_include))
endforeach

foreach t: ['check_plugin', 'check_posix']
    test(t, executable(t, t + '.c',
                       dependencies: [check],
                       link_with: [librobinhood, librbh_posix],
                       include_directories: rbh_include))
endforeach
