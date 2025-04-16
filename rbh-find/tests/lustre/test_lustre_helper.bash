#!/usr/bin/env bash

# This file is part of the RobinHood Library
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_helper()
{
    rbh_find --help lustre ||
        error "Find's helper with lustre extension should have succeeded"

    rbh_find --help lustre | grep "Lustre" ||
        error "Find's helper with lustre extension should have shown Lustre predicates"

    rbh_find --help lustre | grep "POSIX" ||
        error "Find's helper with lustre extension should have shown POSIX predicates"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_helper)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
