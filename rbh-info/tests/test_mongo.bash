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

test_mongo_source()
{
    rbh_sync "rbh:retention:." "rbh:$db:$testdb"
    rbh_sync "rbh:$db:$testdb" "rbh:$db:$testdb-2"

    local output=$(rbh_info "rbh:$db:$testdb" -b)
    local output2=$(rbh_info "rbh:$db:$testdb-2" -b)

    if [ "$output" != "$output2" ]; then
        error "Sync between two databases should result in both having the"
              "same source backends"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

drop_db2()
{
    do_db drop "$testdb-2"
}

declare -a tests=(test_mongo_source)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

sub_teardown=drop_db2
run_tests ${tests[@]}
