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

test_resync()
{
    local entry="test_entry"
    local n_mirror=2
    lfs mirror create -N$n_mirror $entry
    echo "test_data" >> $entry

    invoke_rbh-fsevents

    clear_changelogs "$LUSTRE_MDT" "$userid"
    lfs mirror resync $entry

    local old_version=$(mongo "$testdb" --eval \
        'db.entries.find({"ns.name":"'$entry'"},
                         {"statx.ctime":0, "statx.blocks":0, "xattrs":0})')

    invoke_rbh-fsevents

    local entries=$(mongo "$testdb" --eval "db.entries.find()" | wc -l)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi

    local updated_version=$(mongo "$testdb" --eval \
        'db.entries.find({"ns.name":"'$entry'"},
                         {"statx.ctime":0, "statx.blocks":0, "xattrs":0})')

    if [ "$old_version" != "$updated_version" ]; then
        error "Layout event modified other statx elements than ctime, mtime "
              "and size"
    fi

    find_attribute '"statx.ctime.sec":NumberLong('$(statx +%Z "$entry")')' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.ctime.nsec":0' '"ns.name":"'$entry'"'
    find_attribute '"statx.blocks":NumberLong('$(statx +%b "$entry")')' \
                   '"ns.name":"'$entry'"'

    find_attribute '"xattrs.mirror_count":'$n_mirror '"ns.name":"'$entry'"'
    find_attribute '"xattrs.mirror_id":'$(get_mirror_id "$entry") \
                   '"ns.name":"'$entry'"'
    find_attribute '"xattrs.flags":'$(get_flags "$entry") \
                   '"ns.name":"'$entry'"'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_resync)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
