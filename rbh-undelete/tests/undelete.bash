#!/usr/bin/env bash

# This file is part of rbh-undelete.
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

tests_undelete()
{
    echo "undelete test"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_undelete)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
