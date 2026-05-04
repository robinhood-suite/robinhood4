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

test_stripe_count()
{
    touch a
    lfs setstripe -E -1 -c 2 -S 256k b
    lfs setstripe -E 1M -c 1 -S 256k -E -1 -c 2 -S 512k c
    lfs setstripe -E 1M -c 1 -E 2M -c 2 \
                  -E 4M -c 3 -E 8M -c 4 \
                  -E 16M -c 5 -E -1 -c 6 d
    lfs mkdir e
    lfs setstripe -c 7 e

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    # No idea why 'd' has these stripe count, but it's what Lustre shows with
    # a getstripe too so ...
    rbh_find "rbh:$db:$testdb" -printf "%p: '%RLc'\n" | sort |
        difflines "/: 'None'" "/a: '[1]'" "/b: '[2]'" "/c: '[1, 2]'" \
                  "/d: '[1, 1, 2, 4, 5, 6]'" "/e: '[7]'"
}

test_stripe_size()
{
    touch a
    lfs setstripe -E -1 -c 2 -S 256k b
    lfs setstripe -E 1M -c 1 -S 256k -E -1 -c 2 -S 512k c
    lfs setstripe -E 1M -S 128k -E 2M -S 256k \
                  -E 4M -S 512k -E 8M -S 1M \
                  -E 16M -S 2M -E -1 -S 4M d
    lfs mkdir e
    lfs setstripe -S 16M e

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: '%RLs'\n" | sort |
        difflines "/: 'None'" "/a: '[1048576]'" "/b: '[262144]'" \
                  "/c: '[262144, 524288]'" \
                  "/d: '[131072, 262144, 524288, 1048576, 2097152, 4194304]'" \
                  "/e: '[16777216]'"
}

test_hash_type()
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

    rbh_find "rbh:$db:$testdb" -printf "%p: %RLh\n" | sort |
        difflines "/: None" \
                  "/test_mdt_hash1: all_char" \
                  "/test_mdt_hash2: fnv_1a_64" \
                  "/test_mdt_hash3: crush" \
                  "/test_mdt_hash4: crush2"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_fid test_gen test_ost test_pfid test_stripe_count
                  test_stripe_size test_hash_type)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
