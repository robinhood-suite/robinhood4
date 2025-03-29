#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash

if ! lctl get_param mdt.*.hsm_control | grep "enabled"; then
    echo "At least 1 MDT needs to have HSM control enabled" >&2
    exit 77
fi

################################################################################
#                                    TESTS                                     #
################################################################################

test_hsm()
{
    local entry="$1"

    invoke_rbh-fsevents

    local entries=$(count_documents)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be only $count entries in the database"
    fi

    find_attribute '"statx.ctime.sec":NumberLong("'$(statx +%Z "$entry")'")' \
                   '"ns.name":"'$entry'"'
    find_attribute '"statx.ctime.nsec":0' '"ns.name":"'$entry'"'
    find_attribute '"statx.blocks":NumberLong("'$(statx +%b "$entry")'")' \
                   '"ns.name":"'$entry'"'

    local id=$(lfs hsm_state "$entry" | cut -d ':' -f3)
    find_attribute '"ns.name":"'$entry'"' \
                   '"xattrs.hsm_archive_id":'$id \
                   '"xattrs.hsm_state":'$(get_hsm_state "$entry")

    find_attribute '"xattrs.trusted.lov":{$exists: true}' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.trusted.hsm":{$exists: true}' '"ns.name":"'$entry'"'
}

test_hsm_archive()
{
    local entry="test_entry"
    touch $entry
    archive_file $entry

    test_hsm $entry
}

test_hsm_release()
{
    local entry="test_entry"
    touch $entry
    archive_file $entry

    invoke_rbh-fsevents

    hsm_release_file $entry

    test_hsm $entry
}

test_hsm_restore()
{
    local entry="test_entry"
    touch $entry
    archive_file $entry
    hsm_release_file $entry

    invoke_rbh-fsevents

    hsm_restore_file $entry

    test_hsm $entry
}

test_hsm_remove()
{
    local entry="test_entry"
    touch $entry
    archive_file $entry

    invoke_rbh-fsevents

    hsm_remove_file $entry

    test_hsm $entry
}

test_hsm_remove_and_rm()
{
    local entry="test_entry"
    touch $entry
    archive_file $entry
    hsm_remove_file $entry
    rm $entry

    invoke_rbh-fsevents

    local entries=$(count_documents)
    local count=$(find . | wc -l)
    if [[ $entries -ne $count ]]; then
        error "There should be $count entries in the database, found $entries"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_hsm_archive test_hsm_release test_hsm_restore
                  test_hsm_remove test_hsm_remove_and_rm)

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
