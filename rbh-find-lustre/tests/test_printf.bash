#!/usr/bin/env bash

# This file is part of the RobinHood Library
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later


test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_id()
{
    touch fileA
    touch fileB

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    local IDA=$(rbh_lfind "rbh:$db:$testdb" -name fileA -printf "%I\n")
    local IDB=$(rbh_lfind "rbh:$db:$testdb" -name fileB -printf "%I\n")

    local countA="$(mongo "$testdb" --eval \
                    "db.entries.countDocuments({\"_id\": BinData(0, \"$IDA\")})")"
    local countB="$(mongo "$testdb" --eval \
                    "db.entries.countDocuments({\"_id\": BinData(0, \"$IDB\")})")"

    if [[ "$countA" != "1" ]]; then
        error "Couldn't find entry with ID '$IDA'"
    fi

    if [[ "$countB" != "1" ]]; then
        error "Couldn't find entry with ID '$IDB'"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_id)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
