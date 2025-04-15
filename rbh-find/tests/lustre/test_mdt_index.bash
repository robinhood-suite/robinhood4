#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

mdt_count=$(lfs mdts | grep "lustre-MDT" | wc -l)
if [[ $mdt_count -lt 3 ]]; then
    exit 77
fi

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    if rbh_find "rbh:mongo:$testdb" -mdt-index -1; then
        error "find with a negative mdt index should have failed"
    fi

    if rbh_find "rbh:mongo:$testdb" -mdt-index $(echo 2^64 | bc); then
        error "find with an mdt index too big should have failed"
    fi

    if rbh_find "rbh:mongo:$testdb" -mdt-index 42blob; then
        error "find with an invalid mdt index should have failed"
    fi

    if rbh_find "rbh:mongo:$testdb" -mdt-index invalid; then
        error "find with an invalid mdt index should have failed"
    fi
}

test_mdt_index()
{
    lfs setdirstripe -i 0 root

    cd root
    lfs mkdir -i 1 dir1
    lfs setdirstripe -i 2 dir2

    touch dir1/file1
    touch file2

    rbh_sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -mdt-index 1 | sort |
        difflines "/dir1" "/dir1/file1"

    rbh_find "rbh:mongo:$testdb" -mdt-index 0 | sort |
        difflines "/" "/file2"

    rbh_find "rbh:mongo:$testdb" -mdt-index 3 | sort |
        difflines

    rbh_find "rbh:mongo:$testdb" -mdt-index 0 -mdt-index 1 | sort |
        difflines

    rbh_find "rbh:mongo:$testdb" -mdt-index 1 -o -mdt-index 2 | sort |
        difflines "/dir1" "/dir1/file1" "/dir2"

    rbh_find "rbh:mongo:$testdb" -not -mdt-index 1 | sort |
        difflines "/" "/dir2" "/file2"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_mdt_index)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap "rm -r $tmpdir" EXIT

cd "$tmpdir"

run_tests "${tests[@]}"
