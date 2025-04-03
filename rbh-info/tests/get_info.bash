#!/usr/bin/env bash

# This file is part of rbh-info.
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

setup_double_db()
{
    # Create test databases' names
    testdb1=$SUITE-1$test
    testdb2=$SUITE-2$test
}
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

test_collection_backend_source_posix()
{
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local rbh_info_backend_source=$(rbh_info "rbh:mongo:$testdb" -b)
    local -a info_array

    while IFS= read -r line; do
        info_array+=("$line")
    done <<< "$rbh_info_backend_source"

    local mongo_backend_source=$(mongo "$testdb" --eval " db.info.find({},
                               {backend_source:1}).toArray()[0].backend_source")

    mongo_backend_source=$(echo "$mongo_backend_source" | tr -d '[:space:][]""')
    IFS=',' read -r -a mongo_array <<< "$mongo_backend_source"

    if [[ "${info_array[@]}" != "${mongo_array[@]}" ]]; then
        error "sources retrieved from info does not match mongo ones"
    fi
}

test_collection_backend_source_mongo()
{
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb1"
    rbh_sync "rbh:mongo:$testdb1" "rbh:mongo:$testdb2"

    local rbh_info_backend_source=$(rbh_info "rbh:mongo:$testdb2" -b)
    local -a info_array

    while IFS= read -r line; do
        info_array+=("$line")
    done <<< "$rbh_info_backend_source"

    local mongo_backend_source=$(mongo "$testdb2" --eval " db.info.find({},
                               {backend_source:1}).toArray()[0].backend_source")

    mongo_backend_source=$(echo "$mongo_backend_source" | tr -d '[:space:][]""')
    IFS=',' read -r -a mongo_array <<< "$mongo_backend_source"

    if [[ "${info_array[@]}" != "${mongo_array[@]}" ]]; then
        error "sources retrieved from info does not match mongo ones"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_collection_size test_collection_count
                  test_collection_avg_obj_size
                  test_collection_backend_source_posix
                  test_collection_backend_source_mongo)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

sub_setup=setup_double_db
run_tests ${tests[@]}
