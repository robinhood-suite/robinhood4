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

test_report()
{
    touch "empty"
    truncate --size 1K "1K"
    truncate --size 1025 "1K+1"
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local dir_size="$(stat -c %s .)"
    local size="$((1024 + 1025 + dir_size))"

    rbh_report "rbh:mongo:$testdb" --output "sum(statx.size)" |
        difflines "$size"

    truncate --size 600M "600M"
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local size_600M="$(stat -c %s 600M)"
    dir_size="$(stat -c %s .)"
    size="$((1024 + 1025 + size_600M + dir_size))"

    rbh_report "rbh:mongo:$testdb" --output "sum(statx.size)" |
        difflines "$size"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_report)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
