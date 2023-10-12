# This file is part of RobinHood 4
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = [
  'test_actions',
  'test_perm',
  'test_printf',
  'test_size',
  'test_time',
  'test_xattr',
]

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e)
endforeach
