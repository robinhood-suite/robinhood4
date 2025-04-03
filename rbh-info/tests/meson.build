# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#               alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = [
    'info', 'get_info',
]

if liblustre.found()
    integration_tests += ['test_lustre_mongo_backend_source']
endif

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-info', timeout:120)
endforeach
