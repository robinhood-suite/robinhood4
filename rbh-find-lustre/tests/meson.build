# This file is part of RobinHood 4
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = ['test_compat', 'test_hsm_state', 'test_fid',
                     'test_ost_index']

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e)
endforeach

e = find_program('test_stripe_count.bash')
test('test_stripe_count', e, should_fail: true)
