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

    rbh_report rbh:$db:blob &&
        error "rbh-report with no '--output' should have failed"

    return 0
}

test_parsing_output()
{
    rbh_report rbh:$db:blob --output &&
        error "rbh-report with '--output' but no string should have failed"

    rbh_report rbh:$db:blob --output "," &&
        error "rbh-report with invalid output string should have failed"

    rbh_report rbh:$db:blob --output "sum" &&
        error "rbh-report with output string missing field should have failed"

    rbh_report rbh:$db:blob --output "sum(" &&
        error "rbh-report with output string missing closing parenthesis" \
              "should have failed"

    rbh_report rbh:$db:blob --output "sum(something" &&
        error "rbh-report with output string missing closing parenthesis" \
              "should have failed"

    rbh_report rbh:$db:blob --output "sum(something)" &&
        error "rbh-report with output string and invalid field" \
              "should have failed"

    rbh_report rbh:$db:blob --output "blob(statx.size)" &&
        error "rbh-report with output string and invalid accumulator" \
              "should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)," &&
        error "rbh-report with invalid output string should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size,)" &&
        error "rbh-report with invalid output string should have failed"

    rbh_report rbh:$db:blob --output ",sum(statx.size)" &&
        error "rbh-report with invalid output string should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size,statx.type)" &&
        error "rbh-report with invalid output string should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size),statx.type" &&
        error "rbh-report with two output components but no accumulator on" \
              "the second one should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size),blob(statx.type)" &&
        error "rbh-report with two output components but invalid accumulator" \
              "on the second one should have failed"

    return 0
}

test_parsing_group()
{
    rbh_report rbh:$db:blob --output "sum(statx.size)" --group &&
        error "rbh-report with '--group' but no string should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)" --group "," &&
        error "rbh-report with invalid group string should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)" --group "blob" &&
        error "rbh-report with invalid field in group string should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)" \
                              --group "statx.type," &&
        error "rbh-report with invalid comma in group string should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)" \
                              --group ",statx.type" &&
        error "rbh-report with invalid comma in group string should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)" \
                              --group "statx.type,blob" &&
        error "rbh-report with invalid field in group string should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)" \
                              --group "statx.type,statx.uid," &&
        error "rbh-report with invalid comma in group string should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)" \
                              --group "statx.type[,statx.uid" &&
        error "rbh-report with missing closing bracket should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)" \
                              --group "statx.type[],statx.uid" &&
        error "rbh-report with empty brackets should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)" \
                              --group "statx.type[,statx.uid,]" &&
        error "rbh-report with brackets containing invalid values" \
              "should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)" \
                              --group "statx.type[statx.uid]" &&
        error "rbh-report with brackets containing invalid values" \
              "should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)" \
                              --group "statx.type[;],statx.uid" &&
        error "rbh-report with brackets containing semi-colon but no values" \
              "should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)" \
                              --group "statx.type[10;],statx.uid" &&
        error "rbh-report with invalid values in brackets should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)" \
                              --group "statx.type[;10],statx.uid" &&
        error "rbh-report with invalid values in brackets should have failed"

    rbh_report rbh:$db:blob --output "sum(statx.size)" \
                              --group "statx.type[10;blob],statx.uid" &&
        error "rbh-report with invalid values in brackets should have failed"

    return 0
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_parsing test_parsing_group test_parsing_output)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
