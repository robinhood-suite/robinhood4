#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

setup_files() {
    export dir1="test_dir1"
    local dir2="test_dir2"
    export file1="test_file1"
    local file2="test_file2"
    local file3="test_file3"
    local file4="test_file4"
    local file5="test_file5"
    local file6="test_file6"
    local file7="test_file7"
    export file8="test_file8"
    local block="test_block"
    local fifo="test_fifo"
    local char="test_char"
    local socket="test_socket"

    touch $file1
    mkdir $dir1
    touch $dir1/$file2
    ln -s $dir1/$file2 $file3
    ln $file1 $dir1/$file4
    mknod $block b 1 2
    mknod $dir1/$fifo p
    mknod $char c 1 2
    python3 -c "import socket as s; \
                sock = s.socket(s.AF_UNIX); \
                sock.bind('$socket')"

    mv $dir1/$file2 $dir1/$file5

    archive_file $file1
    setfattr -n user.test -v 42 $file1

    touch $file6
    lfs migrate -E 1k -c 2 -E -1 -c 1 $file6

    lfs setdirstripe -i 1 $dir2
    lfs migrate -m 0 $dir2
    lfs setstripe -c 2 -S 3M $dir2

    lfs mirror create -N2 $file7
    truncate -s 300 $file7
    lfs mirror resync $file7

    touch $file8
}

acceptance_workers()
{
    setup_files

    local output="$(rbh_fsevents -v --nb-workers 3 -b 5 \
        --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT"?ack-user=$userid "rbh:$db:$testdb" )"

    for i in 0 1 2; do
        echo "$output" | grep "Starting consumer thread: $i"
    done

    for entry in $(find *); do
        verify_statx $entry
        verify_lustre $entry
    done

    clear_changelogs "$LUSTRE_MDT" "$userid"

    rm -rf $dir1
    # This entry should stay in the database even after a rm since it has valid
    # copy archived
    rm $file1
    rm $file8

    output="$(rbh_fsevents -v --nb-workers 3 -b 5 \
        --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT"?ack-user=$userid "rbh:$db:$testdb")"

    for i in 0 1 2; do
        echo "$output" | grep "Starting consumer thread: $i"
    done

    local entries=$(count_documents)
    local count=$(find . | wc -l)
    # +1 for $file1 which is still in the DB
    count=$((count + 1))
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database, " \
              "found $entries"
    fi
}

acceptance_workers_no_dedup()
{
    setup_files

    local output="$(rbh_fsevents -v --nb-workers 3 -b 0 \
        --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT"?ack-user=$userid "rbh:$db:$testdb")"

    for i in 0 1 2; do
        echo "$output" | grep "Starting consumer thread: $i"
    done

    for entry in $(find *); do
        verify_statx $entry
        verify_lustre $entry
    done

    clear_changelogs "$LUSTRE_MDT" "$userid"

    rm -rf $dir1
    # This entry should stay in the database even after a rm since it has valid
    # copy archived
    rm $file1
    rm $file8

    output="$(rbh_fsevents -v --nb-workers 3 -b 0 \
        --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT"?ack-user=$userid "rbh:$db:$testdb")"

    for i in 0 1 2; do
        echo "$output" | grep "Starting consumer thread: $i"
    done

    local entries=$(count_documents)
    local count=$(find . | wc -l)
    # +1 for $file1 which is still in the DB
    count=$((count + 1))
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database, " \
              "found $entries"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(acceptance_workers acceptance_workers_no_dedup)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

sub_setup=lustre_setup
sub_teardown=lustre_teardown
run_tests "${tests[@]}"
