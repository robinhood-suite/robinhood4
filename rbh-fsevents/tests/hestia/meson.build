# This file is part of the rbh-fsevents
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = ['test_create', 'test_remove', 'test_read', 'test_update',
                     'acceptance']

foreach t: integration_tests
    e = find_program(t + '.bash')
    # Meson by default runs all tests in parallel, and since they all add data
    # to the same filesystem, it may lead to unintentionnal consequences on the
    # test results. To prevent this, we forbid Meson from running tests
    # in parallel.
    test(t, e, is_parallel : false)
endforeach
