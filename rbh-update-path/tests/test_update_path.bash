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

test_concurrent_rename()
{
    mongo_only_test

    tmp_gdb_script=$(mktemp)

    mkdir dir
    mkdir dir/subdir
    touch dir/subdir/file

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"
    local userid="$(lctl --device "$LUSTRE_MDT" changelog_register |
                        cut -d "'" -f2)"

    cat <<EOF > "$tmp_gdb_script"
set breakpoint pending on
break generate_fsevent_update_path
commands
shell mv dir new_dir
shell $__rbh_fsevents -e rbh:lustre:$LUSTRE_DIR \
    src:lustre:$LUSTRE_MDT?ack-user=$userid rbh:$db:$testdb
del 1
continue
end
run
EOF

    mv dir/subdir dir/new_subdir

    rbh_fsevents -e "rbh:lustre:$LUSTRE_DIR" \
        "src:lustre:$LUSTRE_MDT?ack-user=$userid" "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort | difflines "/" "/dir" "/dir/subdir/file"

    # We rename dir while rbh-update is updating the path of new_subdir, as
    # new_subdir have children, dir should also be updated by rbh-update.
    DEBUGINFOD_URLS="" gdb -batch -x "$tmp_gdb_script" \
        --args $__rbh_update_path rbh:$db:$testdb

    lctl --device "$LUSTRE_MDT" changelog_deregister "$userid"

    rbh_find "rbh:$db:$testdb" | sort |
        difflines "/" "/new_dir" "/new_dir/new_subdir" \
            "/new_dir/new_subdir/file"
}

test_multiple_arborescence()
{
    for i in {1..5}; do
        mkdir "dir$i"
        touch "dir$i/file"
        touch "dir$i/file_bis"
    done

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    local expected="$(echo "/"; find . -type f -printf "/%P\n" | sort)"

    local userid="$(lctl --device "$LUSTRE_MDT" changelog_register |
                        cut -d "'" -f2)"

    for i in {1..5}; do
        mv "dir$i" "new_dir$i"
    done

    rbh_fsevents -e "rbh:lustre:$LUSTRE_DIR" \
        "src:lustre:$LUSTRE_MDT?ack-user=$userid" "rbh:$db:$testdb"

    lctl --device "$LUSTRE_MDT" changelog_deregister "$userid"

    rbh_find "rbh:$db:$testdb" | sort | difflines "$expected"

    rbh_update_path "rbh:$db:$testdb"

    expected="$(find . -printf "/%P\n" | sort)"
    rbh_find "rbh:$db:$testdb" | sort | difflines "$expected"
}

test_config()
{
    mongo_only_test

    local conf_file="conf"

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

    # invalid config
    echo "---
 mongo:
     address: \"mongodb://localhost:12345\"
---" > $conf_file

    rbh_update_path --config $conf_file "rbh:$db:$testdb" &&
        error "rbh-update-path with invalid server address in config should" \
              "have failed"

    # valid config
    echo "---
 mongo:
     address: \"mongodb://localhost:27017\"
---" > $conf_file

    rbh_update_path --config $conf_file "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" | sort | difflines "/" "/new_dir" "/new_dir/file"
}

################################################################################
#                                     MAIN                                     #
################################################################################
declare -a tests=(test_simple_update_path test_multiple_update_path
                  test_2_depth_update_path test_hardlink_update_path
                  test_concurrent_rename test_multiple_arborescence
                  test_config)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests "${tests[@]}"
