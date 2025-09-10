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

    if [ "$rbh_info_size" != "$formated_db_size" ]; then
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
    mongo_only_test

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local rbh_info_avg_obj_size=$(rbh_info "rbh:$db:$testdb" -a)
    local db_avg_obj_size=$(do_db avgsize "$testdb")
    local formated_avg_obj_size_db=$(format_size "$db_avg_obj_size")

    if [ "$rbh_info_avg_obj_size" != "$formated_avg_obj_size_db" ]; then
        error "average objects size are not matching\n"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_collection_size test_collection_count
                  test_collection_avg_obj_size)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
