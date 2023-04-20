#!/usr/bin/env bash

# This file is part of rbh-fsevents
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

ost_count=$(lfs osts | wc -l)
if [[ $ost_count -lt 2 ]]; then
    exit 77
fi

################################################################################
#                                    TESTS                                     #
################################################################################

test_flrw()
{
    local entry="test_entry"
    lfs mirror create -N2 $entry

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    clear_changelogs
    echo "test_data" >> $entry

    local old_version=$(mongo "$testdb" --eval \
        'db.entries.find({"ns.name":"'$entry'"},
                         {"statx.ctime":0, "statx.mtime":0, "statx.size":0,
                          "statx.blocks":0})')

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" --lustre "$LUSTRE_MDT" \
        "rbh:mongo:$testdb"

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi

    local updated_version=$(mongo "$testdb" --eval \
        'db.entries.find({"ns.name":"'$entry'"},
                         {"statx.ctime":0, "statx.mtime":0, "statx.size":0,
                          "statx.blocks":0})')

    if [ "$old_version" != "$updated_version" ]; then
        error "Layout event modified more than ctime, mtime and size"
    fi

    find_attribute '"statx.ctime.sec":NumberLong('$(statx +%Z "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.ctime.nsec":0' '"ns.name":"'$entry'"'
    find_attribute '"statx.mtime.sec":NumberLong('$(statx +%Y "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.mtime.nsec":0' '"ns.name":"'$entry'"'
    find_attribute '"statx.size":NumberLong('$(statx +%s "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.blocks":NumberLong('$(statx +%b "$entry")')' \
                   '"ns.name":"'$entry'"'

    #TODO: enrich this test with Lustre specific testing
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_flrw)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
start_changelogs "$LUSTRE_MDT"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; clear_changelogs" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
