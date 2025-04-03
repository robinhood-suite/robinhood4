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

test_collection_backend_source_lustre()
{
    rbh_sync "rbh:lustre:/mnt/lustre" "rbh:mongo:$testdb"

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

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_collection_backend_source_lustre)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
