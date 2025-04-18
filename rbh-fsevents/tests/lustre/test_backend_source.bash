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

test_posix()
{
    local entry="test_entry"

    touch $entry

    ulimit -c unlimited
    rbh_fsevents --enrich rbh:posix:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb"

    clear_changelogs "$LUSTRE_MDT" "$userid"

    local backend_source=$(do_db source "$testdb")

    if ! echo "$backend_source" | grep "posix"; then
        error "Source backend should contain POSIX plugin"
    fi
}

test_retention()
{
    local entry="test_entry"
    local conf_file="config"

    cat << EOF > $conf_file
RBH_RETENTION_XATTR: "user.blob"
backends:
    retention:
        extends: posix
        enrichers:
            - retention
EOF

    touch $entry

    rbh_fsevents --config $conf_file --enrich rbh:retention:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb"

    clear_changelogs "$LUSTRE_MDT" "$userid"

    local backend_source=$(do_db source "$testdb")

    if ! echo "$backend_source" | grep "posix" | grep "retention"; then
        error "Source backend should contain POSIX plugin and retention extension"
    fi
}

test_lustre()
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

    touch $entry

    rbh_fsevents --config $conf_file --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb"

    clear_changelogs "$LUSTRE_MDT" "$userid"

    local backend_source=$(do_db source "$testdb")

    if ! echo "$backend_source" | grep "posix" | grep "lustre" |
                                  grep "retention"; then
        error "Source backend should contain POSIX plugin and Lustre and retention extension"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_posix test_retention test_lustre)

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
