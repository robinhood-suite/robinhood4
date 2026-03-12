#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_composite()
{
    mongo_only_test

    touch file1
    lfs mirror create -N2 file2

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -composite | sort |
        difflines "/file2"
    rbh_find "rbh:$db:$testdb" -not -composite | sort |
        difflines "/" "/file1"
    rbh_find "rbh:$db:$testdb" -composite -or -not -composite | sort |
        difflines "/" "/file1" "/file2"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_composite)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
