# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

integration_tests = [
    'test',
]

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-gc', timeout: 120)
endforeach
