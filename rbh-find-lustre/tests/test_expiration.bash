#!/usr/bin/env bash

# This file is part of rbh-find-lustre
# Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

set -e

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

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
    local fileE="expired_file_E"
    local timeE="inf"

    touch "$fileA"
    setfattr -n user.expires -v "$timeA" "$fileA"

    touch "$fileB"
    setfattr -n user.expires -v "$timeB" "$fileB"

    touch "$fileC"
    setfattr -n user.expires -v "$timeC" "$fileC"

    touch "$fileD"
    setfattr -n user.expires -v "$timeD" "$fileD"

    touch "$fileE"
    setfattr -n user.expires -v "$timeE" "$fileE"

    rbh_sync "rbh:lustre:." "rbh:mongo:$testdb"

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
    rbh_lfind "rbh:mongo:$testdb" -expired-at 18446744073709551615 | sort |
        difflines "/$fileA" "/$fileB" "/$fileC" "/$fileD"
    rbh_lfind "rbh:mongo:$testdb" -expired-at +18446744073709551614 | sort |
        difflines
    rbh_lfind "rbh:mongo:$testdb" -expired-at inf | sort |
        difflines "/$fileE"
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
    setfattr -n user.expires -v "$timeA" "$fileA"
    touch -t "$(date +%m%d%H%M.%S --date='-35 seconds')" "$fileA"

    touch "$fileB"
    setfattr -n user.expires -v "$timeB" "$fileB"
    touch -t "$(date +%m%d%H%M.%S --date='-105 seconds')" "$fileB"

    touch "$fileC"
    setfattr -n user.expires -v "$timeC" "$fileC"

    rbh_sync "rbh:lustre:." "rbh:mongo:$testdb"

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

test_printf_expiration_info()
{
    local fileA="fileA"
    local timeA="+30"
    local fileB="fileB"
    local timeB="3"
    local fileC="fileC"
    local timeC="inf"
    local fileD="fileD"

    touch "$fileA" "$fileB" "$fileC" "$fileD"
    setfattr -n user.expires -v "$timeA" "$fileA"
    setfattr -n user.expires -v "$timeB" "$fileB"
    setfattr -n user.expires -v "$timeC" "$fileC"

    rbh_sync "rbh:lustre:." "rbh:mongo:$testdb"

    local expA="$(stat -c %X $fileA)"
    expA="$(date -d "@$((expA + timeA))" "+%a %b %e %T %Y")"
    local expB="$(date -d "@$timeB" "+%a %b %e %T %Y")"

    rbh_lfind "rbh:mongo:$testdb" -printf "%p %E\n" | sort |
        difflines "/ None" "/$fileA $expA" "/$fileB $expB" "/$fileC Inf" \
                  "/$fileD None"

    rbh_lfind "rbh:mongo:$testdb" -printf "%p %e\n" | sort |
        difflines "/ None" "/$fileA $timeA" "/$fileB $timeB" "/$fileC $timeC" \
                  "/$fileD None"
}

test_config()
{
    local dir="dir"
    local fileA="fileA"
    local fileB="fileB"
    local conf_file="conf_file"

    echo "---
RBH_RETENTION_XATTR: \"user.blob\"
---" > $conf_file

    mkdir $dir
    touch $dir/$fileA
    touch $dir/$fileB

    setfattr -n user.blob -v 42 $dir/$fileA
    setfattr -n user.expires -v 777 $dir/$fileB

    rbh_sync --config $conf_file "rbh:lustre:$dir" "rbh:mongo:$testdb"

    rbh_lfind --config $conf_file "rbh:mongo:$testdb" -printf "%p %e\n" | sort |
        difflines "/ None" "/$fileA 42" "/$fileB None"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_expired_abs test_expired_rel
                  test_printf_expiration_info test_config)

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap "rm -r $tmpdir" EXIT

cd "$tmpdir"

run_tests "${tests[@]}"
