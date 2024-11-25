#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_parsing()
{
    rbh_report &&
        error "rbh-report with no URI should have failed"

    rbh_report rbh:mongo:blob &&
        error "rbh-report with no '--output' should have failed"

    rbh_report rbh:mongo:blob --output &&
        error "rbh-report with '--output' but no string should have failed"

    rbh_report rbh:mongo:blob --output "," &&
        error "rbh-report with invalid output string should have failed"

    rbh_report rbh:mongo:blob --output "sum(something" &&
        error "rbh-report with output string missing closing parenthesis" \
              "should have failed"

    rbh_report rbh:mongo:blob --output "blob(statx.size)" &&
        error "rbh-report with output string and invalid accumulator" \
              "should have failed"

    rbh_report rbh:mongo:blob --output "sum(something)" &&
        error "rbh-report with output string and invalid field" \
              "should have failed"

    rbh_report rbh:mongo:blob --output "sum(statx.size)," &&
        error "rbh-report with invalid output string should have failed"

    rbh_report rbh:mongo:blob --output ",sum(statx.size)" &&
        error "rbh-report with invalid output string should have failed"

    return 0
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_parsing)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
