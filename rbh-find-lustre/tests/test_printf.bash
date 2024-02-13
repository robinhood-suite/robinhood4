#!/usr/bin/env bash

# This file is part of the RobinHood Library
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

set -xe

if ! command -v rbh-sync &> /dev/null; then
    echo "This test requires rbh-sync to be installed" >&2
    exit 1
fi

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_id()
{
    touch fileA
    touch fileB
    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    local IDA=$(rbh-lfind "rbh:mongo:$testdb" -name fileA -printf "%I\n")
    local IDB=$(rbh-lfind "rbh:mongo:$testdb" -name fileB -printf "%I\n")

    local countA="$(mongo --quiet "$testdb" --eval \
                    "db.entries.find({\"_id\": BinData(0, \"$IDA\")}).count()")"
    local countB="$(mongo --quiet "$testdb" --eval \
                    "db.entries.find({\"_id\": BinData(0, \"$IDB\")}).count()")"

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
