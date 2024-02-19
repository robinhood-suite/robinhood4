#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../test_utils.bash
. $test_dir/lustre_utils.bash

difflines()
{
    diff -y - <([ $# -eq 0 ] && printf '' || printf '%s\n' "$@")
}

find_attribute()
{
    old_IFS=$IFS
    IFS=','
    local output="$*"
    IFS=$old_IFS
    local res=$(mongo $testdb --eval "db.entries.count({$output})")
    [[ "$res" == "1" ]] && return 0 ||
        error "No entry found with filter '$output'"
}

retention_teardown()
{
    hwclock --hctosys
    lustre_teardown
}

################################################################################
#                                    TESTS                                     #
################################################################################

test_retention()
{
    local entry="test_entry"

    touch $entry

    local path_without_mount="$(realpath $entry)"
    path_without_mount="${path_without_mount#"$LUSTRE_DIR"}"

    setfattr -n user.ccc_expires -v +5 $entry

    invoke_rbh-fsevents

    clear_changelogs "$LUSTRE_MDT" "$userid"

    verify_lustre "$entry"

    mongo $testdb --eval "db.entries.find()"
    local exp_time="$(( $(stat -c %X $entry) + 5))"
    find_attribute '"xattrs.user.ccc_expiration_date": NumberLong('$exp_time')'\
                   '"ns.name": "'$entry'"'

    rbh-lfind "rbh:mongo:$testdb" -expired | sort | difflines

    date --set="@$(( $(stat -c %X $entry) + 6))"

    rbh-lfind "rbh:mongo:$testdb" -expired | sort |
        difflines "/$path_without_mount"

    truncate -s 300 $entry

    invoke_rbh-fsevents

    verify_lustre "$entry"

    mongo $testdb --eval "db.entries.find()"
    exp_time="$(( $(stat -c %Y $entry) + 5))"
    find_attribute '"xattrs.user.ccc_expiration_date": NumberLong('$exp_time')'\
                   '"ns.name": "'$entry'"'

    rbh-lfind "rbh:mongo:$testdb" -expired | sort | difflines

    date --set="@$(( $(stat -c %Y $entry) + 6))"

    rbh-lfind "rbh:mongo:$testdb" -expired | sort |
        difflines "/$path_without_mount"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_retention)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

run_tests lustre_setup retention_teardown "${tests[@]}"
