#!/usr/bin/env bash

# This file is part of rbh-undelete.
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_simple_update_path()
{
    mkdir dir
    touch dir/file

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    local userid="$(lctl --device "$LUSTRE_MDT" changelog_register |
                        cut -d "'" -f2)"

    mv dir new_dir

    rbh_fsevents -e "rbh:lustre:." "src:lustre:$LUSTRE_MDT?ack-user=$userid" \
        "rbh:$db:$testdb"

    lctl --device "$LUSTRE_MDT" changelog_deregister "$userid"

    rbh_find "rbh:$db:$testdb" | sort | difflines "/" "/dir/file"

    rbh_update "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort | difflines "/" "/new_dir" "/new_dir/file"
}

################################################################################
#                                     MAIN                                     #
################################################################################
declare -a tests=(test_simple_update_path)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests "${tests[@]}"
