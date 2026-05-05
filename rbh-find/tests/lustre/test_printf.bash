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

test_pool()
{
    local file="pool_file"
    local file2="pool_file2"

    lfs setstripe -E 1M -p "test_pool1" -E 2G -p ""  -E -1 -p "test_pool2" \
        "$file"
    truncate -s 2M "$file"
    lfs setstripe -p "test_pool3" "$file2"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: %RLP\n" | sort |
        difflines "/: None" \
                  "/pool_file2: [test_pool3]" \
                  "/pool_file: [test_pool1, , test_pool2]"
}

test_project_id()
{
    touch "project_1"
    touch "project_2"
    touch "vanilla"

    lfs project -p 1 "project_1"
    lfs project -p 2 "project_2"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: %RLI\n" | sort |
        difflines "/: 0" "/project_1: 1" "/project_2: 2" "/vanilla: 0"
}

test_ost_mdt_count()
{

    lfs mkdir -c 1 dir1
    lfs mkdir -c 2 dir2
    lfs mkdir -c 3 dir3

    lfs setstripe -o 1 a
    lfs setstripe -i 0 -E 1M -c 2 -S 256k -E -1 -c 2 -S 512k b
    lfs setstripe -E 1M -o 0,2,3 -c 3 -S 256k -E -1 -c 2 -S 512k c

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: %RLC\n" | sort |
        difflines "/: 1" \
                  "/a: 1" \
                  "/b: 2" \
                  "/c: 3" \
                  "/dir1: 1" \
                  "/dir2: 2" \
                  "/dir3: 3"
}

test_mdt_index()
{
    lfs setdirstripe -i 0 root

    cd root
    lfs mkdir -i 1 dir1
    lfs setdirstripe -i 2 dir2

    touch dir1/file1
    touch file2

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: %RLi\n" | sort |
        difflines "/: 0" \
                  "/dir1/file1: 1" \
                  "/dir1: 1" \
                  "/dir2: 2" \
                  "/file2: 0"
}

test_layout_pattern()
{
    lfs setstripe -L raid0 -c 1 raid0
    lfs setstripe -L raid0 -c 1 .
    lfs setstripe -L mdt -E 1M mdt

    lfs setstripe -C 5 overstriped

    touch released
    archive_file released
    lfs hsm_release released

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: '%RLt'\n" | sort |
        difflines "/: '[raid0]'" \
                  "/mdt: '[mdt]'" \
                  "/overstriped: '[overstriped]'" \
                  "/raid0: '[raid0]'" \
                  "/released: '[raid0|released]'"
}

test_comp_flags()
{
    lfs setstripe -E 1M -S 256k -E 2M -S 1M -E -1 -S 1G file0
    truncate -s 1M "file0"

    lfs setstripe --extension-size 64M -c 1 -E -1 file1

    lfs mirror create -N -Eeof -c2 -o0,1 -N -Eeof -c2 -o1,2 file2
    echo "blob" > file2

    lfs setstripe -E 1M -S 256k -E 2M -S 1M -E -1 -S 1G file3
    lfs setstripe --comp-set -I 1 --comp-flags=nosync file3

    lfs setstripe -E 1M -S 256k -E -1 -S 1M file4
    lfs setstripe --comp-set -I 1 --comp-flags=prefer file4

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: '%RLl'\n" | sort |
        difflines "/: 'None'" \
                  "/file0: '[init, init, 0]'" \
                  "/file1: '[init, extension]'" \
                  "/file2: '[init, init|stale]'" \
                  "/file3: '[init|nosync, nosync, nosync]'" \
                  "/file4: '[init|prefer, 0]'"
}

test_extension_size()
{
    lfs setstripe --extension-size 64M -c 1 -E -1 file1
    lfs setstripe --extension-size 256M -c 1 -E -1 file2

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: '%RLe'\n" | sort |
        difflines "/: 'None'" \
                  "/file1: '[0, 67108864]'" \
                  "/file2: '[0, 268435456]'"
}

test_comp_start_end()
{
    lfs setstripe -E 1M -c 1 -S 256k -E 512M -c 2 -S 512k -E -1 -c -1 -S 1024k \
        file
    truncate -s 1M file

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p: '%RLb'\n" | sort |
        difflines "/: 'None'" \
                  "/file: '[0, 1048576, 536870912]'"

    rbh_find "rbh:$db:$testdb" -printf "%p: '%RLE'\n" | sort |
        difflines "/: 'None'" \
                  "/file: '[1048576, 536870912, -1]'"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_fid test_gen test_ost test_pfid test_stripe_count
                  test_stripe_size test_hash_type test_pool test_project_id
                  test_ost_mdt_count test_mdt_index test_layout_pattern
                  test_comp_flags test_extension_size test_comp_start_end)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
