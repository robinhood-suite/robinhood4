#!/usr/bin/env bash

# This file is part of rbh-fsevents
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_create_file()
{
    touch "test_file"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    if [[ $entries -ne 2 ]]; then
        error "There should be only two entries in the database"
    fi

    find_attribute "\"ns.name\":\"test_file\""
    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file\""
}

test_create_two_files()
{
    touch "test_file1"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    find_attribute "\"ns.name\":\"test_file1\""
    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file1\""

    clear_changelogs
    touch "test_file2"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    find_attribute "\"ns.name\":\"test_file1\""
    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file1\""
    find_attribute "\"ns.name\":\"test_file2\""
    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file2\""
}

test_create_check_statx_attr()
{
    echo "blob" > "test_file"

    rbh_fsevents --enrich "$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    find_attribute "\"ns.name\":\"test_file\""
    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/test_file\""
    find_attribute "\"statx.atime.sec\":NumberLong($(statx +%X 'test_file'))" \
                   "\"ns.name\":\"test_file\""
    find_attribute "\"statx.atime.nsec\":0" "\"ns.name\":\"test_file\""
    find_attribute "\"statx.ctime.sec\":NumberLong($(statx +%Z 'test_file'))" \
                   "\"ns.name\":\"test_file\""
    find_attribute "\"statx.ctime.nsec\":0" "\"ns.name\":\"test_file\""
    find_attribute "\"statx.mtime.sec\":NumberLong($(statx +%Y 'test_file'))" \
                   "\"ns.name\":\"test_file\""
    find_attribute "\"statx.mtime.nsec\":0" "\"ns.name\":\"test_file\""
    find_attribute "\"statx.btime.nsec\":0" "\"ns.name\":\"test_file\""
    find_attribute "\"statx.blksize\":$(statx +%o 'test_file')" \
                   "\"ns.name\":\"test_file\""
    find_attribute "\"statx.blocks\":NumberLong($(statx +%b 'test_file'))" \
                   "\"ns.name\":\"test_file\""
    find_attribute "\"statx.nlink\":$(statx +%h 'test_file')" \
                   "\"ns.name\":\"test_file\""
    find_attribute "\"statx.ino\":NumberLong(\"$(statx +%i 'test_file')\")" \
                   "\"ns.name\":\"test_file\""
    find_attribute "\"statx.gid\":$(statx +%g 'test_file')" \
                   "\"ns.name\":\"test_file\""
    find_attribute "\"statx.uid\":$(statx +%u 'test_file')" \
                   "\"ns.name\":\"test_file\""
    find_attribute "\"statx.size\":NumberLong($(statx +%s 'test_file'))" \
                   "\"ns.name\":\"test_file\""

    local raw_mode="$(statx +%f 'test_file' 16)"
    local type=$((raw_mode & 00170000))
    local mode=$((raw_mode & ~00170000))
    find_attribute "\"statx.type\":$type" "\"ns.name\":\"test_file\""
    find_attribute "\"statx.mode\":$mode" "\"ns.name\":\"test_file\""

    # TODO: I do not know how to retrieve these values: btime (stat doesn't give
    # what is recorded in the db, maybe a bug) and device numbers

    #find_attribute "\"statx.btime.sec\":NumberLong($(statx +%W 'test_file'))"
    #find_attribute "\"statx.dev.major\":$(statx +%d 'test_file')"
    #find_attribute "\"statx.dev.minor\":$(statx +%d 'test_file')"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_create_file test_create_two_files
                  test_create_check_statx_attr)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
