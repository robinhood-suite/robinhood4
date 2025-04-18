#!/usr/bin/env bash

# This file is part of rbh-info.
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_collection_size()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local rbh_info_size=$(rbh_info "rbh:$db:$testdb" -s)
    local db_size=$(do_db size "$testdb")

    if [ "$rbh_info_size" != "$db_size" ]; then
        error "sizes are not matching\n"
    fi
}

test_collection_count()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local rbh_info_count=$(rbh_info "rbh:$db:$testdb" -c)
    local db_count=$(do_db count "$testdb")

    if [ "$rbh_info_count" != "$db_count" ]; then
        error "count are not matching\n"
    fi
}

test_collection_avg_obj_size()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local rbh_info_avg_obj_size=$(rbh_info "rbh:$db:$testdb" -a)
    local db_avg_obj_size=$(do_db avgsize "$testdb")

    if [ "$rbh_info_avg_obj_size" != "$db_avg_obj_size" ]; then
        error "average objects size are not matching\n"
    fi
}

test_collection_sync()
{
    local info_flag=$1

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local output=$(rbh_info "rbh:mongo:$testdb" "$info_flag")
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != 7)); then
        error "There should be seven infos about posix sync"
    fi

    echo "$output" | grep "sync_debut" ||
        error "sync_debut should have been retrieved"

    echo "$output" | grep "sync_duration" ||
        error "sync_duration should have been retrieved"

    echo "$output" | grep "sync_end" ||
        error "sync_end should have been retrieved"

    echo "$output" | grep "converted_entries" ||
        error "converted_entries should have been retrieved"

    local mountpoint=$(awk -F': ' '/mountpoint/ {print $2}' <<< "$output")

    if [ ! -e "$mountpoint" ]; then
        error "Unvalid mountpoint"
    fi

    local command_line=$(grep 'command_line' <<< "$output" |
                         sed -n 's/.*rbh-sync/rbh-sync/p')

    if [ "$command_line" != "rbh-sync rbh:posix:. rbh:mongo:$testdb" ];
    then
        error "command lines are not matching"
    fi

    echo "$output" | grep "skipped_entries" ||
        error "skipped_entries should have been retrieved"
}

test_collection_first_sync() {
    test_collection_sync "--first-sync"
}

test_collection_last_sync() {
    test_collection_sync "--last-sync"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_collection_size test_collection_count
                  test_collection_avg_obj_size test_collection_first_sync
                  test_collection_last_sync)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
