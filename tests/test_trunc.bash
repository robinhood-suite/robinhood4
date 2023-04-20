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

test_truncate()
{
    local entry="test_entry"
    # if the file does not exist, truncate will also create it, but that will
    # trigger a creat/close changelog, so to only test the truncate event, we
    # first create a file, sync it to the DB, then do a truncate
    touch $entry

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    clear_changelogs

    # retrieve the version before the truncate and after, to check there is no
    # difference between the two except the fields that should be modified
    local old_version=$(mongo "$testdb" --eval \
        'db.entries.find({"ns.name":"'$entry'"},
                         {"statx.mtime":0, "statx.ctime":0, "statx.size":0})')
    truncate -s 300 $entry

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi

    local updated_version=$(mongo "$testdb" --eval \
        'db.entries.find({"ns.name":"'$entry'"},
                         {"statx.mtime":0, "statx.ctime":0, "statx.size":0})')

    if [ "$old_version" != "$updated_version" ]; then
        error "Truncate modified more than mtime, ctime and size"
    fi

    find_attribute '"statx.ctime.sec":NumberLong('$(statx +%Z "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.ctime.nsec":0' '"ns.name":"'$entry'"'
    find_attribute '"statx.mtime.sec":NumberLong('$(statx +%Y "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.mtime.nsec":0' '"ns.name":"'$entry'"'
    find_attribute '"statx.size":NumberLong('$(statx +%s "$entry")')' \
                   '"ns.name":"'$entry'"'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_truncate)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'; clear_changelogs" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
