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

check_statx()
{
    local entry="$1"

    find_attribute '"ns.name":"'$entry'"'
    # XXX: to uncomment once the path is enriched
    # find_attribute "\"ns.xattrs.path\":\"/${testdir#*lustre/}/$entry\""
    find_attribute '"statx.atime.sec":NumberLong('$(statx +%X "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.atime.nsec":0' '"ns.name":"'$entry'"'
    find_attribute '"statx.ctime.sec":NumberLong('$(statx +%Z "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.ctime.nsec":0' '"ns.name":"'$entry'"'
    find_attribute '"statx.mtime.sec":NumberLong('$(statx +%Y "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.mtime.nsec":0' '"ns.name":"'$entry'"'
    find_attribute '"statx.btime.nsec":0' '"ns.name":"'$entry'"'
    find_attribute '"statx.blksize":'$(statx +%o "$entry") \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.blocks":NumberLong('$(statx +%b "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.nlink":'$(statx +%h "$entry") \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.ino":NumberLong("'$(statx +%i "$entry")'")' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.gid":'$(statx +%g "$entry") \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.uid":'$(statx +%u "$entry") \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.size":NumberLong('$(statx +%s "$entry")')' \
                   '"ns.name":"'$entry'"'

    local raw_mode="$(statx +%f "$entry" 16)"
    local type=$((raw_mode & 00170000))
    local mode=$((raw_mode & ~00170000))
    find_attribute '"statx.type":'$type '"ns.name":"'$entry'"'
    find_attribute '"statx.mode":'$mode '"ns.name":"'$entry'"'
}

acceptance()
{
    local dir1="test_dir1"
    local dir2="test_dir2"
    local file1="test_file1"
    local file2="test_file2"
    local file3="test_file3"
    local file4="test_file4"
    local file5="test_file5"
    local file6="test_file6"
    local file7="test_file7"
    local file8="test_file8"
    local nod1="test_nod1"
    local nod2="test_nod2"

    touch $file1
    mkdir $dir1
    touch $dir1/$file2
    ln -s $dir1/$file2 $file3
    ln $file1 $dir1/$file4
    mknod $nod1 b 1 2
    mknod $dir1/$nod2 p
    mv $dir1/$file2 $dir1/$file5

    hsm_archive_file $file1
    setfattr -n user.test -v 42 $file1

    touch $file6
    lfs migrate -E 1k -c 2 -E -1 -c 1 $file6

    lfs setdirstripe -i 1 $dir2
    lfs migrate -m 0 $dir2

    lfs mirror create -N2 $file7
    truncate -s 300 $file7
    lfs mirror resync $file7

    touch $file8

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    for entry in "$(find *)"; do
        check_statx $entry
    done

    clear_changelogs

    rm -rf $dir1
    # This entry should stay in the database even after a rm since it has valid
    # copy archived
    rm $file1
    rm $file8

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    mongo $testdb --eval "db.entries.find()"
    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    # +1 for $file1 which is still in the DB
    count=$((count + 1))
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(acceptance)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; clear_changelogs" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
