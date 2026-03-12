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

test_invalid()
{
    touch "none"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -hash-type 42 &&
        error "find with an invalid hash type should have failed"

    rbh_find "rbh:$db:$testdb" -hash-type all_char_blob &&
        error "find with an invalid hash type should have failed"

    return 0
}

test_types()
{
    lfs setstripe -H all_char file0
    lfs setstripe -H fnv_1a_64 file1

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -hash-type all_char | sort |
        difflines "/" "/file0"

    rbh_find "rbh:$db:$testdb" -hash-type fnv_1a_64 | sort |
        difflines "/file1"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_types)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
