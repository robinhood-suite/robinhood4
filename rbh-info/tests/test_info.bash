#!/usr/bin/env bash

# This file is part of RobinHood.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_all()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local output="$(rbh_info "rbh:$db:$testdb" --all)"

    echo "$output" | grep "Bytes"
    echo "$output" | grep "Plugin"
    echo "$output" | grep "posix"
    echo "$output" | grep "1"
    echo "$output" | grep "Last sync start date"
    echo "$output" | grep "start" | grep "date"
    echo "$output" | grep "test_info-test_all"
}

format_size()
{
    local size=$1
    local KB=1024

    if ((size < KB)); then
        echo "${size} Bytes"
    else
        awk -v s="$size" -v kb="$KB" 'BEGIN { printf "%.2f KB\n", s/kb}'
    fi
}

test_collection_size()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local rbh_info_size=$(rbh_info "rbh:$db:$testdb" -s)
    local db_size=$(do_db size "$testdb")
    local formated_db_size=$(format_size "$db_size")

    if [[ ! "$rbh_info_size" == *"$formated_db_size"* ]]; then
        error "sizes are not matching. Expected $formated_db_size, got $rbh_info_size"
    fi
}

test_collection_count()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local rbh_info_count=$(rbh_info "rbh:$db:$testdb" -c)
    local db_count=$(do_db count "$testdb")

    if [[ ! "$rbh_info_count" == *"$db_count"* ]]; then
        error "count are not matching"
    fi
}

test_collection_avg_obj_size()
{
    mongo_only_test

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local rbh_info_avg_obj_size=$(rbh_info "rbh:$db:$testdb" -a)
    local db_avg_obj_size=$(do_db avgsize "$testdb")
    local formated_avg_obj_size_db=$(format_size "$db_avg_obj_size")

    if [[ ! "$rbh_info_avg_obj_size" == *"$formated_avg_obj_size_db"* ]]; then
        error "average objects size are not matching"
    fi
}

test_collection_mountpoint()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local found_mountpoint=$(rbh_info "rbh:$db:$testdb" -m)

    if [[ ! "$found_mountpoint" == *"$(pwd)"* ]]; then
        error "Mountpoints don't match, found '$found_mountpoint', expected '$(pwd)'\n"
    fi

    rbh_sync "rbh:posix:/tmp" "rbh:$db:$testdb-2"

    local found_mountpoint=$(rbh_info "rbh:$db:$testdb-2" -m)

    do_db drop $testdb-2

    if [[ ! "$found_mountpoint" == *"/tmp"* ]]; then
        error "Mountpoints don't match, found '$found_mountpoint', expected '/tmp'\n"
    fi
}

test_command_backend()
{
    mongo_only_test

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local command_backend=$(rbh_info "rbh:$db:$testdb" -B)

    if [[ ! "$command_backend" == *"posix"* ]]; then
        error "Command backends don't match, found '$command_backend', expected 'posix'\n"
    fi

    command_backend=$(rbh_info "rbh:$db:$testdb" --command-backend)

    if [[ ! "$command_backend" == *"posix"* ]]; then
        error "Command backends don't match, found '$command_backend', expected 'posix'\n"
    fi
}

test_last_sync_start_date()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local output=$(rbh_info "rbh:$db:$testdb" --last-sync-start-date)

    echo "output = '$output'"
    echo "$output" | grep "Last sync start date" ||
        error "last_sync_start_date should have been retrieved"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_all test_collection_size test_collection_count
                  test_collection_avg_obj_size test_collection_mountpoint
                  test_command_backend test_last_sync_start_date)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
