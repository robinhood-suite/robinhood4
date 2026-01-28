#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_setxattr()
{
    local entry="test_file"
    touch "$entry"

    invoke_rbh-fsevents

    clear_changelogs "$LUSTRE_MDT" "$userid"
    setfattr -n user.test -v 42 "$entry"

    invoke_rbh-fsevents

    verify_statx "$entry"
    find_attribute '"xattrs.user":{$exists: true}' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.user.test":{$exists: true}' '"ns.name":"'$entry'"'
}

test_setxattr_remove()
{
    local entry="test_file"
    touch "$entry"

    invoke_rbh-fsevents

    clear_changelogs "$LUSTRE_MDT" "$userid"
    setfattr -n user.test -v 42 "$entry"

    invoke_rbh-fsevents

    verify_statx "$entry"
    find_attribute '"xattrs.user":{$exists: true}' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.user.test":{$exists: true}' '"ns.name":"'$entry'"'

    clear_changelogs "$LUSTRE_MDT" "$userid"
    setfattr --remove="user.test" "$entry"

    invoke_rbh-fsevents

    verify_statx "$entry"
    find_attribute '"xattrs.user":{$exists: true}' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.user.test":{$exists: false}' '"ns.name":"'$entry'"'
}

test_check_last_setxattr_is_inserted()
{
    local entry="test_file"
    touch "$entry"

    invoke_rbh-fsevents

    clear_changelogs "$LUSTRE_MDT" "$userid"
    setfattr -n user.test -v 42 "$entry"
    setfattr -n user.blob -v 43 "$entry"

    invoke_rbh-fsevents

    verify_statx "$entry"
    find_attribute '"xattrs.user":{$exists: true}' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.user.test":{$exists: true}' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.user.blob":{$exists: true}' '"ns.name":"'$entry'"'
}

test_setxattr_replace()
{
    local entry="test_file"
    touch "$entry"

    invoke_rbh-fsevents

    clear_changelogs "$LUSTRE_MDT" "$userid"
    setfattr -n user.test -v 42 "$entry"

    invoke_rbh-fsevents

    find_attribute '"xattrs.user":{$exists: true}' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.user.test":{$exists: true}' '"ns.name":"'$entry'"'
    local old_value=$(do_db get "$testdb" \
        '"xattrs.user.test":{$exists: true}' '_id: 0, "xattrs.user.test": 1')

    clear_changelogs "$LUSTRE_MDT" "$userid"
    setfattr -n user.test -v 43 "$entry"

    invoke_rbh-fsevents

    find_attribute '"xattrs.user":{$exists: true}' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.user.test":{$exists: true}' '"ns.name":"'$entry'"'
    local new_value=$(do_db get "$testdb" \
        '"xattrs.user.test":{$exists: true}' '_id: 0, "xattrs.user.test": 1')

    if [ "$old_value" == "$new_value" ]; then
        error "xattrs values should be different after an update:" \
            "old and new are both equal to $old_value"
    fi
}

test_setxattr_with_typing()
{
    local entry="test_file"
    local conf_file="conf"

    touch "$entry"

    echo "---
backends:
    lustre:
        extends: posix
        enrichers:
            - lustre
xattrs_map:
    user.blob: int32
---" > $conf_file

    setfattr -n user.blob -v 42 "$entry"

    rbh_fsevents --config $conf_file --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT"?ack-user=$userid "rbh:$db:$testdb"

    find_attribute '"xattrs.user":{$exists: true}' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.user.blob": 42' '"ns.name":"'$entry'"'

    echo "---
backends:
    lustre:
        extends: posix
        enrichers:
            - lustre
xattrs_map:
    user.blob: string
---" > $conf_file

    setfattr -n user.blob -v toto "$entry"

    rbh_fsevents --config $conf_file --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT"?ack-user=$userid "rbh:$db:$testdb"

    find_attribute '"xattrs.user":{$exists: true}' '"ns.name":"'$entry'"'
    ! find_attribute '"xattrs.user.blob": 42' '"ns.name":"'$entry'"'
    find_attribute '"xattrs.user.blob": "toto"' '"ns.name":"'$entry'"'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_setxattr test_setxattr_remove
                  test_check_last_setxattr_is_inserted test_setxattr_replace
                  test_setxattr_with_typing)

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
