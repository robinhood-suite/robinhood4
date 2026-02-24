#!/usr/bin/env bash

# This file is part of RobinHood4
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

SUITE=${BASH_SOURCE##*/}
SUITE=${SUITE%.*}

test_dir=$(dirname $(readlink -e $0))
current_path=$PWD
cd $test_dir/../../../builddir/
. $PWD/../utils/tests/framework.bash
cd $current_path

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

    rbh_sync rbh:retention:. "rbh:$db:$testdb"

    local output="$(rbh_update_retention "rbh:$db:$testdb")"
    local output_lines="$(echo "$output" | grep "No directory has expired" | \
                          wc -l)"
    if [ "$output_lines" != "1" ]; then
        error "The 4 directories shouldn't have expired yet: '$output'"
    fi

    date --set="@$(( $(stat -c %X $dir3) + 6))"

    echo "Test: 1 expired, 3 not expired, 0 updated"
    output="$(rbh_update_retention "rbh:$db:$testdb")"
    shant_be_expired "$output" "$dir1"
    shant_be_expired "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    shant_be_expired "$output" "$dir4"
    echo "Success"

    date --set="@$(( $(stat -c %X $dir1) + 11))"

    echo "Test: 2 expired, 2 not expired, 0 updated"
    output="$(rbh_update_retention "rbh:$db:$testdb")"
    should_be_expired "$output" "$dir1"
    shant_be_expired "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    shant_be_expired "$output" "$dir4"
    echo "Success"

    touch -a $dir1/$entry1 $dir2/$entry3
    rbh_sync rbh:retention:. "rbh:$db:$testdb"

    echo "Test: 1 expired, 2 not expired, 1 updated"
    output="$(rbh_update_retention "rbh:$db:$testdb")"
    should_be_updated "$output" "$dir1"
    shant_be_expired "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    shant_be_expired "$output" "$dir4"
    echo "Success"

    date --set="@$(( $(stat -c %X $dir1/$entry1) + 11))"

    echo "Test: 3 expired, 0 not expired, 1 updated"
    output="$(rbh_update_retention "rbh:$db:$testdb")"
    should_be_expired "$output" "$dir1"
    should_be_updated "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    should_be_expired "$output" "$dir4"
    echo "Success"

    date --set="@$(( $(stat -c %X $dir2/$entry3) + 16))"

    echo "Test: 4 expired, 0 not expired, 0 updated"
    output="$(rbh_update_retention "rbh:$db:$testdb")"
    should_be_expired "$output" "$dir1"
    should_be_expired "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    should_be_expired "$output" "$dir4"
    echo "Success"

    touch -a $dir3
    rbh_sync rbh:retention:. "rbh:$db:$testdb"

    # Nothing should have changed because we only updated the access time of the
    # directory, and not its modify time
    echo "Test: 4 expired, 0 not expired, 0 updated"
    output="$(rbh_update_retention "rbh:$db:$testdb")"
    should_be_expired "$output" "$dir1"
    should_be_expired "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    should_be_expired "$output" "$dir4"
    echo "Success"

    touch -m $dir3
    rbh_sync rbh:retention:. "rbh:$db:$testdb"

    echo "Test: 3 expired, 0 not expired, 1 updated"
    output="$(rbh_update_retention "rbh:$db:$testdb")"
    should_be_expired "$output" "$dir1"
    should_be_expired "$output" "$dir2"
    shant_be_expired "$output" "$dir3"
    should_be_expired "$output" "$dir4"
    echo "Success"

    date --set="@$(( $(stat -c %X $dir3) + 6))"

    echo "Test: 4 deleted"
    rbh_update_retention "rbh:$db:$testdb" --delete

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

    rbh_sync rbh:retention:. "rbh:$db:$testdb"

    find_attribute \
        '"xattrs.trusted.expiration_date":NumberLong("'$expiration_date'")' \
        '"ns.xattrs.path":"'/$dir'"'

    date --set="@$(( $(stat -c %Y $dir) + 11))"

    touch -m $dir/$entry
    rbh_sync rbh:retention:. "rbh:$db:$testdb"

    output="$(rbh_update_retention "rbh:$db:$testdb")"
    should_be_updated "$output" "$dir"

    expiration_date="$(( $(stat -c %Y $dir/$entry) + 11))"
    find_attribute \
        '"xattrs.trusted.expiration_date":NumberLong("'$expiration_date'")' \
        '"ns.xattrs.path":"'/$dir'"'

    rbh_sync rbh:retention:. "rbh:$db:$testdb"

    # The expiration date of the directory shouldn't have changed after the
    # sync
    find_attribute \
        '"xattrs.trusted.expiration_date":NumberLong("'$expiration_date'")' \
        '"ns.xattrs.path":"'/$dir'"'

    date --set="@$(( $(stat -c %Y $dir) + 5))"

    touch $dir/$entry2
    rbh_sync rbh:retention:. "rbh:$db:$testdb"

    # The expiration shouldn't have changed as the created file $entry2 is too
    # old to change it
    find_attribute \
        '"xattrs.trusted.expiration_date":NumberLong("'$expiration_date'")' \
        '"ns.xattrs.path":"'/$dir'"'

    date --set="@$(( $(stat -c %Y $dir) + 15))"

    touch $dir/$entry3
    rbh_sync rbh:retention:. "rbh:$db:$testdb"

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
backends:
    retention:
        extends: posix
        enrichers:
            - retention
---" > $conf_file

    local expiration_date="$(( $(stat -c %Y $dir) + 10))"

    rbh_sync rbh:retention:. "rbh:$db:$testdb" --config $conf_file

    find_attribute \
        '"xattrs.trusted.expiration_date":NumberLong("'$expiration_date'")' \
        '"ns.xattrs.path":"'/$dir'"'

    date --set="@$(( $(stat -c %Y $dir) + 11))"

    local output="$(rbh_update_retention "rbh:$db:$testdb")"
    echo "$output" | grep -q "Skipping"

    output="$(rbh_update_retention "rbh:$db:$testdb" --config $conf_file)"
    should_be_expired "$output" "$dir"

    touch $dir/$entry
    rbh_sync rbh:retention:. "rbh:$db:$testdb" --config $conf_file

    output="$(rbh_update_retention "rbh:$db:$testdb")"
    echo "$output" | grep -q "Skipping"

    output="$(rbh_update_retention "rbh:$db:$testdb" --config $conf_file)"
    should_be_updated "$output" "$dir"
}

test_retention_on_empty_dir()
{
    local dir1="dir1"
    local dir2="dir2"
    local dir3="dir3"

    mkdir -p $dir1/$dir2/$dir3

    setfattr -n user.expires -v +5 $dir1

    rbh_sync rbh:retention:. "rbh:$db:$testdb"

    local output="$(rbh_update_retention "rbh:$db:$testdb")"
    local output_lines="$(echo "$output" | grep "No directory has expired" | \
                          wc -l)"
    if [ "$output_lines" != "1" ]; then
        error "Directory '$dir1' shouldn't have expired yet"
    fi

    date --set="@$(( $(stat -c %X $dir1) + 6))"

    output="$(rbh_update_retention "rbh:$db:$testdb" --delete)"
    if [ -d $dir1 ]; then
        error "Directory '$dir1' should have been deleted"
    fi
}

test_retention_expiration_in_days()
{
    local dir="dir"
    local file="file"

    mkdir $dir
    touch $dir/$file

    local current="$(date +%s)"

    setfattr -n user.expires -v "+$(( 5 * 86400 ))" $dir

    rbh_sync rbh:retention:. "rbh:$db:$testdb"

    local output="$(rbh_update_retention "rbh:$db:$testdb" --delay 15)"
    local output_lines="$(echo "$output" | grep "4d:23h:59m" | wc -l)"
    if (( output_lines != 1 )); then
        error "'$dir' should expire in 5 days: '$output'"
    fi

    touch $dir/$file -d "+ 5 days" -m

    rbh_sync rbh:retention:. "rbh:$db:$testdb"

    local output="$(rbh_update_retention "rbh:$db:$testdb" --delay 15)"
    local output_lines="$(echo "$output" | grep "9d:23h:59m" | wc -l)"
    if (( output_lines != 1 )); then
        error "'$dir' should expire in 10 days: '$output'"
    fi

    touch -m $dir/$file -d "+ 15 days"

    rbh_sync rbh:retention:. "rbh:$db:$testdb"

    local output="$(rbh_update_retention "rbh:$db:$testdb" --delay 15)"
    if [ ! -z "$output" ]; then
        error "'$dir' expiration date should have been pushed back without notification: '$output'"
    fi

    date --set="@$(( current + (20 * 86400) ))"
    touch -m $dir
    date --set="@$(( current + (30 * 86400) ))"
    touch -m $dir/$file -d "+ 16 days"

    rbh_sync rbh:retention:. "rbh:$db:$testdb"

    local output="$(rbh_update_retention "rbh:$db:$testdb" --delay 15)"
    local output_lines="$(echo "$output" | grep "Changing" | wc -l)"
    if (( output_lines != 1 )); then
        error "'$dir''s expiration date should have modified: '$output'"
    fi
}

test_retention_sort()
{
    local dir1="dir1"
    local dir2="dir2"
    local dir3="dir3"
    local dir4="dir4"
    # $dir1 and $dir2 appear twice because they are relative, and
    # rbh-update-retention prints them twice over two lines
    local dirs_sorted=("$dir3" "$dir4" "$dir2" "$dir2" "$dir1" "$dir1")

    local current="$(date +%s)"

    mkdir $dir1 $dir2 $dir3 $dir4
    setfattr -n user.expires -v +20 $dir1
    setfattr -n user.expires -v +5 $dir2
    setfattr -n user.expires -v 42 $dir3
    setfattr -n user.expires -v $current $dir4

    date --set="@$(( current + 30 ))"

    rbh_sync rbh:retention:. "rbh:$db:$testdb"

    local output="$(rbh_update_retention "rbh:$db:$testdb")"

    local counter=0
    while IFS= read -r line; do
        echo "$line" | grep "${dirs_sorted[$counter]}"
        counter=$(( counter + 1 ))
    done <<< "$output"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_retention_script test_retention_after_sync
                  test_retention_with_config test_retention_on_empty_dir
                  test_retention_expiration_in_days test_retention_sort)

tmpdir=$(mktemp --directory)
trap "rm -r $tmpdir" EXIT

cd "$tmpdir"

sub_teardown=retention_teardown
run_tests "${tests[@]}"
