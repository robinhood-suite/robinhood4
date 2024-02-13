#!/usr/bin/env bash

# This file is part of RobinHood4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

set -e

SUITE=${BASH_SOURCE##*/}
SUITE=${SUITE%.*}

test_dir=$(dirname $(readlink -e $0))

setup()
{
    # Create a test directory and `cd` into it
    testdir=$PWD/$SUITE-$test
    mkdir "$testdir"
    cd "$testdir"

    # Create test database's name
    testdb=$SUITE-$test
}

teardown()
{
    # Since the test changes the system's date and time, it must be reset at the
    # end of it, which is what hwclock does
    hwclock --hctosys
    mongo --quiet "$testdb" --eval "db.dropDatabase()" >/dev/null
    rm -rf "$testdir"
}

error()
{
    echo "$*"
    exit 1
}

run_tests()
{
    local fail=0

    for test in "$@"; do
        (set -e; trap -- teardown EXIT; setup; "$test")
        if !(($?)); then
            echo "$test: ✔"
        else
            echo "$test: ✖"
            fail=1
        fi
    done

    return $fail
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
        echo "output = '$output'"
        error "Directory '$directory' should be expired"
    fi
}

shant_be_expired()
{
    local output="$1"
    local directory="$2"

    local output_lines="$(echo "$output" | grep "$directory" | grep "expired" |
                          wc -l)"
    if [ "$output_lines" != "0" ]; then
        echo "output = '$output'"
        error "Directory '$directory' shouldn't be expired"
    fi
}

should_be_updated()
{
    local output="$1"
    local directory="$2"

    local output_lines="$(echo "$output" | grep "$directory" | grep "Changing" |
                          wc -l)"
    if [ "$output_lines" != "1" ]; then
        echo "output = '$output'"
        error "Directory '$directory's expiration date should have been updated"
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
    setfattr -n user.ccc_expires -v +10 $dir1
    setfattr -n user.ccc_expires -v +15 $dir2
    setfattr -n user.ccc_expires -v +5 $dir3
    setfattr -n user.ccc_expires -v $(($(stat -c %X $dir4) + 15)) $dir4

    rbh-sync rbh:lustre:. "rbh:mongo:$testdb"

    local output="$($test_dir/rbh_update_retention "rbh:mongo:$testdb")"
    local output_lines="$(echo "$output" | grep "No directory has expired" | \
                          wc -l)"
    if [ "$output_lines" != "1" ]; then
        error "The 4 directories shouldn't have expired yet"
    fi

    date --set="@$(( $(stat -c %X $dir3) + 6))"

    output="$($test_dir/rbh_update_retention "rbh:mongo:$testdb")"
    shant_be_expired "$output" "$dir1"
    shant_be_expired "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    shant_be_expired "$output" "$dir4"

    date --set="@$(( $(stat -c %X $dir1) + 11))"
    rbh-sync rbh:lustre:. "rbh:mongo:$testdb"

    output="$($test_dir/rbh_update_retention "rbh:mongo:$testdb")"
    should_be_expired "$output" "$dir1"
    shant_be_expired "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    shant_be_expired "$output" "$dir4"

    touch -a $dir1/$entry1 $dir2/$entry3
    rbh-sync rbh:lustre:. "rbh:mongo:$testdb"

    output="$($test_dir/rbh_update_retention "rbh:mongo:$testdb")"
    should_be_updated "$output" "$dir1"
    shant_be_expired "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    shant_be_expired "$output" "$dir4"

    date --set="@$(( $(stat -c %X $dir1/$entry1) + 11))"
    rbh-sync rbh:lustre:. "rbh:mongo:$testdb"

    output="$($test_dir/rbh_update_retention "rbh:mongo:$testdb")"
    should_be_expired "$output" "$dir1"
    should_be_updated "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    should_be_expired "$output" "$dir4"

    date --set="@$(( $(stat -c %X $dir2/$entry3) + 16))"
    rbh-sync rbh:lustre:. "rbh:mongo:$testdb"

    output="$($test_dir/rbh_update_retention "rbh:mongo:$testdb")"
    should_be_expired "$output" "$dir1"
    should_be_expired "$output" "$dir2"
    should_be_expired "$output" "$dir3"
    should_be_expired "$output" "$dir4"

    touch -a $dir3
    rbh-sync rbh:lustre:. "rbh:mongo:$testdb"

    output="$($test_dir/rbh_update_retention "rbh:mongo:$testdb")"
    should_be_expired "$output" "$dir1"
    should_be_expired "$output" "$dir2"
    shant_be_expired "$output" "$dir3"
    should_be_expired "$output" "$dir4"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_retention_script)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap "rm -r $tmpdir" EXIT

cd "$tmpdir"

run_tests "${tests[@]}"
