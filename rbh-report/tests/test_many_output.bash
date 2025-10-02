#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_many_output()
{
    for i in {1..4096}; do
        local size=$((1 + $RANDOM % 100))
        truncate -s $size "entry$i"
    done

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_report "rbh:$db:$testdb" \
        --output "avg(statx.size),sum(statx.size),count()" \
        --group-by "name,statx.size"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_many_output)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
