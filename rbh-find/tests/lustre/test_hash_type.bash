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
    lfs mkdir -i 1 "test_mdt_hash1"
    lfs migrate -v -m 0 -H all_char "test_mdt_hash1"

    lfs mkdir -i 1 "test_mdt_hash2"
    lfs migrate -v -m 0 -H fnv_1a_64 "test_mdt_hash2"

    lfs mkdir -i 1 "test_mdt_hash3"
    lfs migrate -v -m 0 -H crush "test_mdt_hash3"

    lfs mkdir -i 1 "test_mdt_hash4"
    lfs migrate -v -m 0 -H crush2 "test_mdt_hash4"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -hash-type all_char | sort |
        difflines "/test_mdt_hash1"

    rbh_find "rbh:$db:$testdb" -hash-type fnv_1a_64 | sort |
        difflines "/test_mdt_hash2"

    rbh_find "rbh:$db:$testdb" -hash-type crush | sort |
        difflines "/test_mdt_hash3"

    rbh_find "rbh:$db:$testdb" -hash-type crush2 | sort |
        difflines "/test_mdt_hash4"
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
