#!/usr/bin/env bash

# This file is part of rbh-find-lustre
# Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

set -e

if ! command -v rbh-sync &> /dev/null; then
    echo "This test requires rbh-sync to be installed" >&2
    exit 1
fi

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    rbh_lfind "rbh:mongo:$testdb" -expired blob &&
        error "find with -expired shouldn't have any argument"

    rbh_lfind "rbh:mongo:$testdb" -expired-at "" &&
        error "find with an invalid expired-at epoch should have failed"

    rbh_lfind "rbh:mongo:$testdb" -expired-at $(echo 2^64 | bc) &&
        error "find with an expired-at epoch too big should have failed"

    rbh_lfind "rbh:mongo:$testdb" -expired-at 42blob &&
        error "find with an invalid expired-at epoch should have failed"

    rbh_lfind "rbh:mongo:$testdb" -expired-at invalid &&
        error "find with an invalid expired-at epoch should have failed"

    rbh_lfind "rbh:mongo:$testdb" -expired-at +$(echo 2^64 | bc) &&
        error "find with an expired epoch-at too big should have failed"

    rbh_lfind "rbh:mongo:$testdb" -expired-at +42blob &&
        error "find with an invalid expired-at epoch should have failed"

    rbh_lfind "rbh:mongo:$testdb" -expired-at +invalid &&
        error "find with an invalid expired-at epoch should have failed"

    rbh_lfind "rbh:mongo:$testdb" -expired-at -$(echo 2^64 | bc) &&
        error "find with an expired-at epoch too big should have failed"

    rbh_lfind "rbh:mongo:$testdb" -expired-at -42blob &&
        error "find with an invalid expired-at epoch should have failed"

    rbh_lfind "rbh:mongo:$testdb" -expired-at -invalid &&
        error "find with an invalid expired-at epoch should have failed"

    return 0
}

test_expired_abs()
{
    local fileA="expired_file_A"
    local timeA="$(($(date +%s) - 30))"
    local fileB="expired_file_B"
    local timeB="$(($(date +%s) + 60))"
    local fileC="expired_file_C"
    local timeC="$(($(date +%s) - 80))"
    local fileD="expired_file_D"
    local timeD="$(($(date +%s) + 100))"

    touch "$fileA"
    setfattr -n user.ccc_expires -v "$timeA" "$fileA"

    touch "$fileB"
    setfattr -n user.ccc_expires -v "$timeB" "$fileB"

    touch "$fileC"
    setfattr -n user.ccc_expires -v "$timeC" "$fileC"

    touch "$fileD"
    setfattr -n user.ccc_expires -v "$timeD" "$fileD"

    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -expired-at $(date +%s) | sort |
        difflines "/$fileA" "/$fileC"
    rbh_lfind "rbh:mongo:$testdb" -expired | sort |
        difflines "/$fileA" "/$fileC"
    rbh_lfind "rbh:mongo:$testdb" -expired-at $timeA | sort |
        difflines "/$fileA" "/$fileC"
    rbh_lfind "rbh:mongo:$testdb" -expired-at +$timeA | sort |
        difflines "/$fileB" "/$fileD"
    rbh_lfind "rbh:mongo:$testdb" -expired-at -$timeD | sort |
        difflines "/$fileA" "/$fileB" "/$fileC"
    rbh_lfind "rbh:mongo:$testdb" -expired-at +$timeD | sort |
        difflines
    rbh_lfind "rbh:mongo:$testdb" -expired-at -$timeC | sort |
        difflines
}

test_expired_rel()
{
    local fileA="expired_file_A"
    local timeA="+30"
    local fileB="expired_file_B"
    local timeB="+100"
    local fileC="expired_file_C"
    local timeC="+50"

    touch "$fileA"
    setfattr -n user.ccc_expires -v "$timeA" "$fileA"
    touch -t "$(date +%m%d%H%M.%S --date='-35 seconds')" "$fileA"

    touch "$fileB"
    setfattr -n user.ccc_expires -v "$timeB" "$fileB"
    touch -t "$(date +%m%d%H%M.%S --date='-105 seconds')" "$fileB"

    touch "$fileC"
    setfattr -n user.ccc_expires -v "$timeC" "$fileC"

    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -expired-at $(date +%s) | sort |
        difflines "/$fileA" "/$fileB"
    rbh_lfind "rbh:mongo:$testdb" -expired | sort |
        difflines "/$fileA" "/$fileB"
    rbh_lfind "rbh:mongo:$testdb" -expired-at +$(date +%s) | sort |
        difflines "/$fileC"
    rbh_lfind "rbh:mongo:$testdb" -expired-at -$(date +%s) | sort |
        difflines "/$fileA" "/$fileB"
    # List all files that will be expired in an hour, which includes $fileA
    # which expired 5 seconds ago, $fileB which expired 5 seconds ago, and
    # $fileC which will be expired in 50 seconds.
    rbh_lfind "rbh:mongo:$testdb" -expired-at -$(date +%s --date='1 hour') |
        sort | difflines "/$fileA" "/$fileB" "/$fileC"
    rbh_lfind "rbh:mongo:$testdb" -expired-at \
        $(($(stat --format='+%X' "$fileA") + timeA)) | sort |
        difflines "/$fileA" "/$fileB"
    rbh_lfind "rbh:mongo:$testdb" \
        -expired-at +$(date +%s) -expired-at -$(date +%s --date='1 minute') |
        sort | difflines "/$fileC"
    rbh_lfind "rbh:mongo:$testdb" \
        -expired-at +$(date +%s --date='-1 hours') -expired-at -$(date +%s) |
        sort | difflines "/$fileA" "/$fileB"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_expired_abs test_expired_rel)

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap "rm -r $tmpdir" EXIT

cd "$tmpdir"

run_tests "${tests[@]}"
