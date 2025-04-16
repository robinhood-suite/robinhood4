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
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local rbh_info_size=$(rbh_info "rbh:mongo:$testdb" -s)
    local mongo_size=$(mongo "$testdb" --eval " db.entries.stats().size")

    if [ "$rbh_info_size" != "$mongo_size" ]; then
        error "sizes are not matching\n"
    fi
}

test_collection_count()
{
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local rbh_info_count=$(rbh_info "rbh:mongo:$testdb" -c)

    if (( $(mongo_version) < $(version_code 5.0.0) )); then
        local mongo_count=$(mongo "$testdb" --eval "db.entries.count()")
    else
        local mongo_count=$(mongo "$testdb" --eval
                            "db.entries.countDocuments()")
    fi

    if [ "$rbh_info_count" != "$mongo_count" ]; then
        error "count are not matching\n"
    fi
}

test_collection_avg_obj_size()
{
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local rbh_info_avg_obj_size=$(rbh_info "rbh:mongo:$testdb" -a)
    local mongo_avg_obj_size=$(mongo "$testdb" --eval "
                               db.entries.stats().avgObjSize")

    if [ "$rbh_info_avg_obj_size" != "$mongo_avg_obj_size" ]; then
        error "average objects size are not matching\n"
    fi
}

test_collection_sync()
{
    local info_flag=$1

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local output=$(rbh_info "rbh:mongo:$testdb" "$info_flag")
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != 5)); then
        error "There should be four infos about posix sync"
    fi

    echo "$output" | grep "sync_debut" ||
        error "sync_debut should have been retrieved"

    echo "$output" | grep "sync_duration" ||
        error "sync_duration should have been retrieved"

    echo "$output" | grep "sync_end" ||
        error "sync_end should have been retrieved"

    echo "$output" | grep "upserted_entries" ||
        error "upserted_entries should have been retrieved"

    echo "$output" | grep "mount_point" ||
        error "mount_point should have been retrieved"
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
