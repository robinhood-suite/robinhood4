#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_layout()
{
    local entry="test_entry"
    touch $entry

    invoke_rbh-fsevents

    clear_changelogs "$LUSTRE_MDT" "$userid"
    lfs migrate -E 1k -c 2 -E -1 -c 1 $entry

    local old_version=$(mongo "$testdb" --eval \
        'db.entries.find({"ns.name":"'$entry'"},
                         {"statx.ctime":0, "xattrs":0})')

    invoke_rbh-fsevents

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be $count entries in the database, found $entries"
    fi

    local updated_version=$(mongo "$testdb" --eval \
        'db.entries.find({"ns.name":"'$entry'"},
                         {"statx.ctime":0, "xattrs":0})')

    if [ "$old_version" != "$updated_version" ]; then
        error "Layout event modified other statx elements than ctime"
    fi

    find_attribute '"statx.ctime.sec":NumberLong('$(statx +%Z "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.ctime.nsec":0' '"ns.name":"'$entry'"'

    verify_lustre "$entry"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_layout)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
