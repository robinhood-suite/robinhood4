#!/usr/bin/env bash

# This file is part of RobinHood4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

SUITE=${BASH_SOURCE##*/}
SUITE=${SUITE%.*}

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../utils/tests/framework.bash

function retention_teardown()
{
    # Since the test changes the system's date and time, it must be reset at the
    # end of it, which is what hwclock does
    hwclock --hctosys
}

################################################################################
#                                    TESTS                                     #
################################################################################

should_be_expired()
{
    local output="$1"
    local directory="$2"

    local output_lines="$(echo "$output" | grep "$directory" | grep "expired" |
                          wc -l)"
    if [ "$output_lines" != "1" ]; then
        error "Directory '$directory' should be expired: '$output'"
    fi
}

shant_be_expired()
{
    local output="$1"
    local directory="$2"

    local output_lines="$(echo "$output" | grep "$directory" | grep "expired" |
                          wc -l)"
    if [ "$output_lines" != "0" ]; then
        error "Directory '$directory' shouldn't be expired: '$output'"
    fi
}

should_be_updated()
{
    local output="$1"
    local directory="$2"

    local output_lines="$(echo "$output" | grep "$directory" | grep "Changing" |
                          wc -l)"
    if [ "$output_lines" != "1" ]; then
        error "Directory '$directory's expiration date should have been updated: '$output'"
    fi
}

test_retention_script()
{
    local dir1="dir1"
    local entry1="entry1"
    local dir2="dir2"
    local entry2="entry2"
    local entry3="entry3"
    local dir3="dir3"
    local dir4="dir4"

    mkdir $dir1 $dir2 $dir3 $dir4
    touch $dir1/$entry1 $dir2/$entry2 $dir2/$entry3
    setfattr -n user.expires -v +10 $dir1
    setfattr -n user.expires -v +15 $dir2
    setfattr -n user.expires -v +5 $dir3
    setfattr -n user.expires -v $(($(stat -c %X $dir4) + 15)) $dir4

    rbh_sync rbh:lustre:. "rbh:mongo:$testdb"

    local output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD")"
    local output_lines="$(echo "$output" | grep "No directory has expired" | \
                          wc -l)"
    if [ "$output_lines" != "1" ]; then
        error "The 4 directories shouldn't have expired yet: '$output'"
    fi

    date --set="@$(( $(stat -c %X $dir3) + 6))"

    echo "Test: 1 expired, 3 not expired, 0 updated"
    output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD")"
    shant_be_expired "$output" "$dir1"
    shant_be_expired "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    shant_be_expired "$output" "$dir4"
    echo "Success"

    date --set="@$(( $(stat -c %X $dir1) + 11))"

    echo "Test: 2 expired, 2 not expired, 0 updated"
    output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD")"
    should_be_expired "$output" "$dir1"
    shant_be_expired "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    shant_be_expired "$output" "$dir4"
    echo "Success"

    touch -a $dir1/$entry1 $dir2/$entry3
    rbh_sync rbh:lustre:. "rbh:mongo:$testdb"

    echo "Test: 1 expired, 2 not expired, 1 updated"
    output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD")"
    should_be_updated "$output" "$dir1"
    shant_be_expired "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    shant_be_expired "$output" "$dir4"
    echo "Success"

    date --set="@$(( $(stat -c %X $dir1/$entry1) + 11))"

    echo "Test: 3 expired, 0 not expired, 1 updated"
    output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD")"
    should_be_expired "$output" "$dir1"
    should_be_updated "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    should_be_expired "$output" "$dir4"
    echo "Success"

    date --set="@$(( $(stat -c %X $dir2/$entry3) + 16))"

    echo "Test: 4 expired, 0 not expired, 0 updated"
    output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD")"
    should_be_expired "$output" "$dir1"
    should_be_expired "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    should_be_expired "$output" "$dir4"
    echo "Success"

    touch -a $dir3
    rbh_sync rbh:lustre:. "rbh:mongo:$testdb"

    # Nothing should have changed because we only updated the access time of the
    # directory, and not its modify time
    echo "Test: 4 expired, 0 not expired, 0 updated"
    output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD")"
    should_be_expired "$output" "$dir1"
    should_be_expired "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    should_be_expired "$output" "$dir4"
    echo "Success"

    touch -m $dir3
    rbh_sync rbh:lustre:. "rbh:mongo:$testdb"

    echo "Test: 3 expired, 0 not expired, 1 updated"
    output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD")"
    should_be_expired "$output" "$dir1"
    should_be_expired "$output" "$dir2"
    shant_be_expired "$output" "$dir3"
    should_be_expired "$output" "$dir4"
    echo "Success"

    date --set="@$(( $(stat -c %X $dir3) + 6))"

    echo "Test: 4 deleted"
    rbh_update_retention "rbh:mongo:$testdb" "$PWD" --delete

    if [ -d "$dir1" ] || [ -d "$dir2" ] || [ -d "$dir3" ] || [ -d "$dir4" ]
    then
      error "All directories should have been deleted."
    fi
    echo "Success"
}

test_retention_after_sync()
{
    local dir="dir"
    local entry="entry"
    local entry2="entry2"
    local entry3="entry3"

    mkdir $dir
    touch $dir/$entry
    setfattr -n user.expires -v +10 $dir

    local expiration_date="$(( $(stat -c %Y $dir) + 10))"

    rbh_sync rbh:lustre:. "rbh:mongo:$testdb"

    find_attribute \
        '"xattrs.trusted.expiration_date":NumberLong("'$expiration_date'")' \
        '"ns.xattrs.path":"'/$dir'"'

    date --set="@$(( $(stat -c %Y $dir) + 11))"

    touch -m $dir/$entry
    rbh_sync rbh:lustre:. "rbh:mongo:$testdb"

    output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD")"
    should_be_updated "$output" "$dir"

    expiration_date="$(( $(stat -c %Y $dir/$entry) + 11))"
    find_attribute \
        '"xattrs.trusted.expiration_date":NumberLong("'$expiration_date'")' \
        '"ns.xattrs.path":"'/$dir'"'

    rbh_sync rbh:lustre:. "rbh:mongo:$testdb"

    # The expiration date of the directory shouldn't have changed after the
    # sync
    find_attribute \
        '"xattrs.trusted.expiration_date":NumberLong("'$expiration_date'")' \
        '"ns.xattrs.path":"'/$dir'"'

    date --set="@$(( $(stat -c %Y $dir) + 5))"

    touch $dir/$entry2
    rbh_sync rbh:lustre:. "rbh:mongo:$testdb"

    # The expiration shouldn't have changed as the created file $entry2 is too
    # old to change it
    find_attribute \
        '"xattrs.trusted.expiration_date":NumberLong("'$expiration_date'")' \
        '"ns.xattrs.path":"'/$dir'"'

    date --set="@$(( $(stat -c %Y $dir) + 15))"

    touch $dir/$entry3
    rbh_sync rbh:lustre:. "rbh:mongo:$testdb"

    # This time the expiration date should be pushed back, with $entry3 being
    # created later down the line
    expiration_date="$(( $(stat -c %Y $dir) + 10))"
    find_attribute \
        '"xattrs.trusted.expiration_date":NumberLong("'$expiration_date'")' \
        '"ns.xattrs.path":"'/$dir'"'
}

test_retention_with_config()
{
    local dir="dir"
    local entry="entry"
    local conf_file="config"

    mkdir $dir
    touch $dir/$entry
    setfattr -n user.blob -v +10 $dir

    echo "---
RBH_RETENTION_XATTR: \"user.blob\"
---" > $conf_file

    local expiration_date="$(( $(stat -c %Y $dir) + 10))"

    rbh_sync rbh:lustre:. "rbh:mongo:$testdb" --config $conf_file

    find_attribute \
        '"xattrs.trusted.expiration_date":NumberLong("'$expiration_date'")' \
        '"ns.xattrs.path":"'/$dir'"'

    date --set="@$(( $(stat -c %Y $dir) + 11))"

    local output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD")"
    echo "$output" | grep -q "Skipping"

    output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD" \
                        --config $conf_file)"
    should_be_expired "$output" "$dir"

    touch $dir/$entry
    rbh_sync rbh:lustre:. "rbh:mongo:$testdb" --config $conf_file

    output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD")"
    echo "$output" | grep -q "Skipping"

    output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD" \
                        --config $conf_file)"
    should_be_updated "$output" "$dir"
}

test_retention_on_empty_dir()
{
    local dir1="dir1"
    local dir2="dir2"
    local dir3="dir3"

    mkdir -p $dir1/$dir2/$dir3

    setfattr -n user.expires -v +5 $dir1

    rbh_sync rbh:lustre:. "rbh:mongo:$testdb"

    local output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD")"
    local output_lines="$(echo "$output" | grep "No directory has expired" | \
                          wc -l)"
    if [ "$output_lines" != "1" ]; then
        error "Directory '$dir1' shouldn't have expired yet"
    fi

    date --set="@$(( $(stat -c %X $dir1) + 6))"

    output="$(rbh_update_retention "rbh:mongo:$testdb" "$PWD" --delete)"
    if [ -d $dir1 ]; then
        error "Directory '$dir1' should have been deleted"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_retention_script test_retention_after_sync
                  test_retention_with_config test_retention_on_empty_dir)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap "rm -r $tmpdir" EXIT

cd "$tmpdir"

sub_teardown=retention_teardown
run_tests "${tests[@]}"
