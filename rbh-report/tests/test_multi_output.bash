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

test_multi_output()
{
    touch first
    touch second
    touch third

    truncate --size 1G first
    truncate --size 3M second
    touch -d "+2 hours" third

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local root_size="$(stat -c %s .)"
    local first_size="$(stat -c %s first)"
    local second_size="$(stat -c %s second)"
    local third_size="$(stat -c %s third)"
    local sum_size="$((root_size + first_size + second_size + third_size))"
    local avg_size="$((sum_size / 4))"

    # FIXME this can fail, the dir is not necessarily the smallest inode
    local min_ino="$(stat -c %i .)"
    local max_mtime="$(stat -c %Y third)"

    rbh_report --csv "rbh:mongo:$testdb" \
        --output "sum(statx.size),avg(statx.size),min(statx.ino),max(statx.mtime.sec)" |
        difflines "$sum_size,$avg_size,$min_ino,$max_mtime"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_multi_output)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
