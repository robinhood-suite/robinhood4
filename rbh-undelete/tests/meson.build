# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#               alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = []

if liblustre.found()
    integration_tests += [ 'undelete' ]
endif

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e, env: backend_path_env, suite: 'rbh-undelete')
endforeach
