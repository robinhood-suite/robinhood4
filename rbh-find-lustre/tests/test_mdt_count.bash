#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

mdt_count=$(lfs mdts | grep "lustre-MDT" | wc -l)
if [[ $mdt_count -lt 3 ]]; then
    exit 77
fi

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    if rbh_find "rbh:mongo:$testdb" -mdt-count $(echo 2^64 | bc); then
        error "find with an mdt count too big should have failed"
    fi

    if rbh_find "rbh:mongo:$testdb" -mdt-count 42blob; then
        error "find with an invalid mdt count should have failed"
    fi

    if rbh_find "rbh:mongo:$testdb" -mdt-count invalid; then
        error "find with an invalid mdt count should have failed"
    fi
}

test_mdt_count()
{
    lfs mkdir -c 1 dir1
    lfs mkdir -c 2 dir2
    lfs mkdir -c 3 dir3

    rbh_sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -mdt-count 1 | sort |
        difflines "/" "/dir1"
    rbh_find "rbh:mongo:$testdb" -mdt-count 0 | sort |
        difflines
    rbh_find "rbh:mongo:$testdb" -mdt-count +3 | sort |
        difflines
    rbh_find "rbh:mongo:$testdb" -mdt-count -3 | sort |
        difflines "/" "/dir1" "/dir2"

    rbh_find "rbh:mongo:$testdb" -mdt-count 0 -mdt-count 1 | sort |
        difflines

    rbh_find "rbh:mongo:$testdb" -mdt-count 1 -o -mdt-count 2 | sort |
        difflines "/" "/dir1" "/dir2"

    rbh_find "rbh:mongo:$testdb" -not -mdt-count 1 | sort |
        difflines "/dir2" "/dir3"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_mdt_count)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap "rm -r $tmpdir" EXIT
cd "$tmpdir"

run_tests "${tests[@]}"
