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

    rbh_fsevents -e "rbh:lustre:$LUSTRE_DIR" \
        "src:lustre:$LUSTRE_MDT?ack-user=$userid" "rbh:$db:$testdb"

    lctl --device "$LUSTRE_MDT" changelog_deregister "$userid"

    rbh_find "rbh:$db:$testdb" | sort | difflines "/" "/dir/file"

    rbh_update_path "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort | difflines "/" "/new_dir" "/new_dir/file"
}

test_multiple_update_path()
{
    mkdir dir
    mkdir dir/subdir1
    mkdir dir/subdir1/subdir2

    touch dir/file
    touch dir/subdir1/file2
    touch dir/subdir1/subdir2/file3

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    local userid="$(lctl --device "$LUSTRE_MDT" changelog_register |
                        cut -d "'" -f2)"

    mv dir new_dir

    rbh_fsevents -e "rbh:lustre:$LUSTRE_DIR" \
        "src:lustre:$LUSTRE_MDT?ack-user=$userid" "rbh:$db:$testdb"

    lctl --device "$LUSTRE_MDT" changelog_deregister "$userid"

    rbh_find "rbh:$db:$testdb" | sort |
        difflines "/" "/dir/file" "/dir/subdir1" "/dir/subdir1/file2" \
            "/dir/subdir1/subdir2" "/dir/subdir1/subdir2/file3"

    rbh_update_path "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort |
        difflines "/" "/new_dir" "/new_dir/file" "/new_dir/subdir1" \
            "/new_dir/subdir1/file2" "/new_dir/subdir1/subdir2" \
            "/new_dir/subdir1/subdir2/file3"
}

test_2_depth_update_path()
{
    mkdir dir
    mkdir dir/subdir

    touch dir/file
    touch dir/subdir/file2

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    local userid="$(lctl --device "$LUSTRE_MDT" changelog_register |
                        cut -d "'" -f2)"

    mv dir/subdir dir/new_subdir
    mv dir new_dir

    rbh_fsevents -e "rbh:lustre:$LUSTRE_DIR" \
        "src:lustre:$LUSTRE_MDT?ack-user=$userid" "rbh:$db:$testdb"

    lctl --device "$LUSTRE_MDT" changelog_deregister "$userid"

    rbh_find "rbh:$db:$testdb" | sort |
        difflines "/" "/dir/file" "/dir/subdir/file2"

    rbh_update_path "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort |
        difflines "/" "/new_dir" "/new_dir/file" "/new_dir/new_subdir" \
            "/new_dir/new_subdir/file2"
}

test_hardlink_update_path()
{
    mkdir dir
    mkdir dir2

    touch dir/file
    ln dir/file dir2/link

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    local userid="$(lctl --device "$LUSTRE_MDT" changelog_register |
                        cut -d "'" -f2)"

    mv dir new_dir

    rbh_fsevents -e "rbh:lustre:$LUSTRE_DIR" \
        "src:lustre:$LUSTRE_MDT?ack-user=$userid" "rbh:$db:$testdb"

    lctl --device "$LUSTRE_MDT" changelog_deregister "$userid"

    rbh_find "rbh:$db:$testdb" | sort |
        difflines "/" "/dir/file" "/dir2" "/dir2/link"

    rbh_update_path "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort |
        difflines "/" "/dir2" "/dir2/link" "/new_dir" "/new_dir/file"
}

################################################################################
#                                     MAIN                                     #
################################################################################
declare -a tests=(test_simple_update_path test_multiple_update_path
                  test_2_depth_update_path test_hardlink_update_path)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests "${tests[@]}"
