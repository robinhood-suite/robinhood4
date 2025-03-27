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

test_count()
{
    for i in {0..25}; do
        mkdir dir$i
        truncate --size $i dir$i/file$i
    done

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_report --csv "rbh:$db:$testdb" --output "count()" |
        difflines "53"

    rbh_report --csv "rbh:$db:$testdb" --group-by "statx.type" \
                                   --output "count()" |
        difflines "directory: 27" "file: 26"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_count)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
