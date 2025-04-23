#!/usr/bin/env bash

# This file is part of RobinHood4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

SUITE=${BASH_SOURCE##*/}
SUITE=${SUITE%.*}

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

function test_parsing
{
    rbh_retention &&
        error "command should have failed because of missing parameters"

    rbh_retention blob &&
        error "command should have failed because 'blob' does not exist"

    touch blob

    rbh_retention blob blob2 &&
        error "command should have failed because 'blob2' is not a valid date"

    rbh_retention blob 123 blob2 &&
        error "command should have failed because of more parameters than necessary"

    rbh_retention --relative &&
        error "command should have failed because of missing parameters"

    return 0
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_parsing)

tmpdir=$(mktemp --directory)
trap "rm -r $tmpdir" EXIT

cd "$tmpdir"

run_tests "${tests[@]}"
