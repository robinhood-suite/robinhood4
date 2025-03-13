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
        error "size are not matching\n"
    fi
}

test_collection_count()
{
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local rbh_info_count=$(rbh_info "rbh:mongo:$testdb" -c)
    local mongo_count=$(mongo "$testdb" --eval " db.entries.countDocuments()")

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

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_collection_size test_collection_count
                  test_collection_avg_obj_size)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
