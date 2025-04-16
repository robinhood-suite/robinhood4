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

test_helper_invalid()
{
    rbh_find --help | grep "POSIX" &&
        error "Find's helper without argument shouldn't have given POSIX's helper"

    rbh_find --help rbh:mongo:blob | grep "POSIX" &&
        error "Find's helper without argument shouldn't have given POSIX's helper"

    rbh_find --help non_existing_backend &&
        error "Find's helper with non-existing backend should have failed"

    return 0
}

test_helper()
{
    rbh_find --help posix | grep "POSIX" ||
        error "Find's helper with POSIX plugin should have succeeded"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_helper_invalid test_helper)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
