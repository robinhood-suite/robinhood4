#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash

difflines()
{
    diff -y - <([ $# -eq 0 ] && printf '' || printf '%s\n' "$@")
}

retention_teardown()
{
    # Since the test changes the system's date and time, it must be reset at the
    # end of it, which is what hwclock does
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

    setfattr -n user.expires -v +5 $entry

    invoke_rbh-fsevents

    clear_changelogs "$LUSTRE_MDT" "$userid"

    verify_lustre "$entry"

    local exp_time="$(( $(stat -c %X $entry) + 5))"
    find_attribute \
        '"xattrs.trusted.expiration_date": NumberLong("'$exp_time'")'\
        '"ns.name": "'$entry'"'
    find_attribute '"xattrs.user.expires": "+5"' '"ns.name": "'$entry'"'

    rbh_find "rbh:mongo:$testdb" -expired | sort | difflines

    date --set="@$(( $(stat -c %X $entry) + 6))"

    rbh_find "rbh:mongo:$testdb" -expired | sort |
        difflines "/$path_without_mount"

    truncate -s 300 $entry

    invoke_rbh-fsevents

    verify_lustre "$entry"

    exp_time="$(( $(stat -c %Y $entry) + 5))"
    find_attribute \
        '"xattrs.trusted.expiration_date": NumberLong("'$exp_time'")'\
        '"ns.name": "'$entry'"'
    find_attribute '"xattrs.user.expires": "+5"' '"ns.name": "'$entry'"'

    rbh_find "rbh:mongo:$testdb" -expired | sort | difflines

    date --set="@$(( $(stat -c %Y $entry) + 6))"

    rbh_find "rbh:mongo:$testdb" -expired | sort |
        difflines "/$path_without_mount"
}

test_config()
{
    local entry="test_entry"
    local conf_file="config"

    cat << EOF > $conf_file
RBH_RETENTION_XATTR: "user.blob"
backends:
    lustre:
        extends: posix
        enrichers:
            - lustre
            - retention
EOF

    clear_changelogs "$LUSTRE_MDT" "$userid"

    touch $entry
    setfattr -n user.blob -v +5 $entry

    local path_without_mount="$(realpath $entry)"
    path_without_mount="${path_without_mount#"$LUSTRE_DIR"}"

    rbh_fsevents --config $conf_file --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:mongo:$testdb"

    local exp_time="$(( $(stat -c %Y $entry) + 5))"
    find_attribute \
        '"xattrs.trusted.expiration_date": NumberLong("'$exp_time'")'\
        '"ns.name": "'$entry'"'
    find_attribute '"xattrs.user.blob": "+5"' '"ns.name": "'$entry'"'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_retention test_config)

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
