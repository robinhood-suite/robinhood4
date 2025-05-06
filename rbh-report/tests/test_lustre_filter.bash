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

test_mdt_index()
{
    lfs setdirstripe -i 0 root

    cd root

    lfs mkdir -i 1 dir1
    lfs mkdir -i 2 dir2

    truncate -s 1024 "dir1/1K"
    truncate -s 1025 "dir2/1K+1"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    local size_dir1="$(stat -c %s dir1)"
    local size_dir2="$(stat -c %s dir2)"

    local size_idx_1="$(($size_dir1 + 1024))"
    local size_idx_2="$(($size_dir2 + 1025))"

    rbh_report --csv "rbh:$db:$testdb" --output "sum(statx.size)" -mdt-index 1 |
        difflines "$size_idx_1"
    rbh_report --csv "rbh:$db:$testdb" --output "sum(statx.size)" -mdt-index 2 |
        difflines "$size_idx_2"
    rbh_report --csv "rbh:$db:$testdb" --output "sum(statx.size)" -mdt-index 1 \
        -o -mdt-index 2 | difflines "$(($size_idx_1 + $size_idx_2))"
    rbh_report --csv "rbh:$db:$testdb" --output "sum(statx.size)" -mdt-index 2 \
        -a -type f | difflines "1025"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_mdt_index)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap "rm -r $tmpdir" EXIT

cd "$tmpdir"

run_tests "${tests[@]}"
