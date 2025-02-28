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

main_user_id="$(id -u)"

test_range()
{
    truncate --size 5 first_smol_file
    truncate --size 5 second_smol_file
    truncate --size 5 third_smol_file

    truncate --size 50 first_large_file
    truncate --size 50 second_large_file
    truncate --size 50 third_large_file

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local root_size="$(stat -c %s .)"
    local smol_size="$(stat -c %s first_smol_file)"
    local large_size="$(stat -c %s first_large_file)"

    if [[ $root_size -lt 30 ]]; then
        local sum_smol_size="$((root_size + (smol_size * 3)))"
        local sum_large_size="$((large_size * 3))"
    else
        local sum_smol_size="$((smol_size * 3))"
        local sum_large_size="$((root_size + (large_size * 3)))"
    fi

    rbh_report --csv "rbh:mongo:$testdb" --group-by "statx.size[0;30]" \
                                         --output "sum(statx.size)" |
        difflines "[0; 30]: $sum_smol_size" "[30; +inf]: $sum_large_size"
}

test_group_by_field_and_range()
{
    truncate --size 5 first_user_first_smol_file
    truncate --size 5 first_user_second_smol_file
    truncate --size 5 first_user_third_smol_file

    truncate --size 50 first_user_first_large_file
    truncate --size 50 first_user_second_large_file
    truncate --size 50 first_user_third_large_file

    truncate --size 20 second_user_first_smol_file
    truncate --size 20 second_user_second_smol_file
    truncate --size 20 second_user_third_smol_file

    truncate --size 150 second_user_first_large_file
    truncate --size 150 second_user_second_large_file
    truncate --size 150 second_user_third_large_file

    local test_user="$(get_test_user)"
    add_test_user $test_user
    fake_user_id="$(id -u $test_user)"
    chown $test_user: second_user_*
    del_test_user $test_user

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local root_size="$(stat -c %s .)"
    local sum_second_smol_size="$((20 * 3))"
    local sum_second_large_size="$((150 * 3))"

    if [[ $root_size -lt 30 ]]; then
        local sum_first_smol_size="$((root_size + (5 * 3)))"
        local sum_first_large_size="$((50 * 3))"
        local expected="$(printf "%s\n%s\n%s\n%s\n" \
                          "$main_user_id,[0; 30]: $sum_first_smol_size"
                          "$main_user_id,[30; 70]: $sum_first_large_size" \
                          "$fake_user_id,[0; 30]: $sum_second_smol_size" \
                          "$fake_user_id,[30; 70]: $sum_second_large_size")"
    elif [[ $root_size -lt 70 ]]; then
        local sum_first_smol_size="$((5 * 3))"
        local sum_first_large_size="$((root_size + (50 * 3)))"
        local expected="$(printf "%s\n%s\n%s\n%s\n" \
                          "$main_user_id,[0; 30]: $sum_first_smol_size" \
                          "$main_user_id,[30; 70]: $sum_first_large_size" \
                          "$fake_user_id,[0; 30]: $sum_second_smol_size" \
                          "$fake_user_id,[30; 70]: $sum_second_large_size")"
    else
        local sum_first_smol_size="$((5 * 3))"
        local sum_first_large_size="$((50 * 3))"
        local expected="$(printf "%s\n%s\n%s\n%s\n%s\n" \
                          "$main_user_id,[0; 30]: $sum_first_smol_size" \
                          "$main_user_id,[30; 70]: $sum_first_large_size" \
                          "$main_user_id,[70; +inf]: $root_size" \
                          "$fake_user_id,[0; 30]: $sum_second_smol_size" \
                          "$fake_user_id,[70; +inf]: $sum_second_large_size")"
    fi

    rbh_report --csv "rbh:mongo:$testdb" \
        --group-by "statx.uid,statx.size[0;30;70]" --output "sum(statx.size)" |
        difflines "$expected"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_range test_group_by_field_and_range)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
