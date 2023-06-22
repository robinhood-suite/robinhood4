#!/usr/bin/env bash

# This file is part of the RobinHood Library
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_sync_2_files()
{
    truncate -s 1k "fileA"

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"
    find_attribute '"ns.xattrs.path":"/"'
    find_attribute '"ns.xattrs.path":"/fileA"'
}

test_sync_size()
{
    truncate -s 1025 "fileA"
    local length=$(stat -c %s "fileA")

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"
    find_attribute '"ns.xattrs.path":"/fileA"' '"statx.size" : '$length
}

test_sync_3_files()
{
    truncate -s 1k "fileA"
    truncate -s 1k "fileB"

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"
    find_attribute '"ns.xattrs.path":"/"'
    find_attribute '"ns.xattrs.path":"/fileA"'
    find_attribute '"ns.xattrs.path":"/fileB"'
}

test_sync_xattrs()
{
    truncate -s 1k "fileA"
    setfattr -n user.a -v b "fileA"
    truncate -s 1k "fileB"
    setfattr -n user.c -v d "fileB"

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"
    find_attribute '"ns.xattrs.path":"/fileA"' \
                   '"xattrs.user.a" : { $exists : true }'
    find_attribute '"ns.xattrs.path":"/fileB"' \
                   '"xattrs.user.c" : { $exists : true }'
}

test_sync_subdir()
{
    mkdir "dir"
    truncate -s 1k "dir/file"
    truncate -s 1k "fileA"
    truncate -s 1k "fileB"

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"
    find_attribute '"ns.xattrs.path":"/"'
    find_attribute '"ns.xattrs.path":"/fileA"'
    find_attribute '"ns.xattrs.path":"/fileB"'
    find_attribute '"ns.xattrs.path":"/dir"'
    find_attribute '"ns.xattrs.path":"/dir/file"'
}

test_sync_large_tree()
{
    mkdir -p {1..9}/{1..9}

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"
    for i in $(find *); do
        find_attribute '"ns.xattrs.path":"/'$i'"'
    done
}

test_sync_one_one_file()
{
    truncate -s 1k "fileA"
    local length=$(stat -c %s "fileA")

    rbh_sync -o "rbh:posix:fileA" "rbh:mongo:$testdb"
    find_attribute '"statx.size" : '$length
}

test_sync_one_two_files()
{
    truncate -s 1k "fileA"
    truncate -s 1k "fileB"
    setfattr -n user.a -v b "fileB"
    local length=$(stat -c %s "fileB")

    rbh_sync -o "rbh:posix:fileA" "rbh:mongo:$testdb"
    rbh_sync -o "rbh:posix:fileB" "rbh:mongo:$testdb"
    find_attribute '"statx.size" : '$length \
                   '"xattrs.user.a" : { $exists : true }'

    local output_lines=$(mongo $testdb --eval "db.entries.count()")
    if [[ $output_lines -ne 2 ]]; then
        error "Invalid number of files were synced, expected '2' lines, " \
              "found '$output_lines'."
    fi
}

check_mode_and_type()
{
    local entry="$1"

    local raw_mode="$(statx -c="+%f" "$entry")"
    raw_mode=${raw_mode:2}
    raw_mode=$(echo "ibase=16; ${raw_mode^^}" | bc)
    local type=$((raw_mode & 00170000))
    local mode=$((raw_mode & ~00170000))

    find_attribute '"statx.type":'$type
    find_attribute '"statx.mode":'$mode
}

test_sync_socket()
{
    local entry="socket_file"

    python -c "import socket as s; \
               sock = s.socket(s.AF_UNIX); \
               sock.bind('$entry')"

    rbh_sync -o "rbh:posix:$entry" "rbh:mongo:$testdb"
    check_mode_and_type $entry
}

test_sync_fifo()
{
    local entry="fifo_file"

    mkfifo $entry

    rbh_sync -o "rbh:posix:$entry" "rbh:mongo:$testdb"
    check_mode_and_type $entry
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_sync_2_files test_sync_size test_sync_3_files
                  test_sync_xattrs test_sync_subdir test_sync_large_tree
                  test_sync_one_one_file test_sync_one_two_files
                  test_sync_socket test_sync_fifo)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
