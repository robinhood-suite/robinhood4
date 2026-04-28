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

test_fid()
{
    mkdir blob
    touch blob/blob

    local root_fid="$(lfs path2fid .)"
    local blob_fid="$(lfs path2fid blob)"
    local blob_blob_fid="$(lfs path2fid blob/blob)"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: '%RLf'\n" | sort |
        difflines "/: '$root_fid'" "/blob/blob: '$blob_blob_fid'" \
                  "/blob: '$blob_fid'"
}

test_gen()
{
    touch a
    lfs setstripe -E -1 -c 2 -S 256k b
    lfs setstripe -E 1M -c 1 -S 256k -E -1 -c 2 -S 512k c
    lfs setstripe -E 1M -c 1 -E 2M -c 2 \
                  -E 4M -c 3 -E 8M -c 4 \
                  -E 16M -c 5 -E -1 -c 6 d

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: '%RLg'\n" | sort |
        difflines "/: 'None'" "/a: '0'" "/b: '1'" "/c: '2'" "/d: '6'"
}

test_ost()
{
    lfs setstripe -o 1 a
    lfs setstripe -i 0 -E 1M -c 2 -S 256k -E -1 -c 2 -S 512k b
    lfs setstripe -E 1M -o 0,2,3 -c 3 -S 256k -E -1 -c 2 -S 512k c

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: '%RLo'\n" | sort |
        difflines "/: 'None'" "/a: '[1]'" "/b: '[0, 1, -1]'" \
                  "/c: '[0, 2, 3, -1]'"
}

test_pfid()
{
    mkdir blob
    touch blob/blob

    local root_fid="$(lfs path2fid .)"
    local blob_fid="$(lfs path2fid blob)"
    local blob_blob_fid="$(lfs path2fid blob/blob)"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: '%RLp'\n" | sort |
        difflines "/: 'None'" "/blob/blob: '$blob_fid'" \
                  "/blob: '$root_fid'"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_fid test_gen test_ost test_pfid)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
