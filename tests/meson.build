# This file is part of the RobinHood Library
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

integration_tests = ['test_posix_mongo_sync', 'test_mongo_mongo_sync']

foreach t: integration_tests
    e = find_program(t + '.bash')
    test(t, e)
endforeach
