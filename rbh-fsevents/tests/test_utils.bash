#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

################################################################################
#                                  UTILITIES                                   #
################################################################################

SUITE=${BASH_SOURCE##*/}
SUITE=${SUITE%.*}

__rbh_fsevents=$(PATH="$PWD:$PATH" which rbh-fsevents)
rbh_fsevents()
{
    "$__rbh_fsevents" "$@"
}

__mongo=$(which mongosh || which mongo)
mongo()
{
    "$__mongo" --quiet "$@"
}


################################################################################
#                                DATABASE UTILS                                #
################################################################################

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

################################################################################
#                                  TEST UTILS                                  #
################################################################################

join_arr() {
    local IFS="$1"

    shift
    echo "$*"
}

build_long_array()
{
    local output="$*"
    local arr=($output)

    for i in "${!arr[@]}"; do
        arr[$i]="NumberLong(${arr[$i]})"
    done

    echo "[$(join_arr ", " ${arr[@]})]"
}

build_string_array()
{
    local output="$*"
    local arr=($output)

    for i in "${!arr[@]}"; do
        if [ -z ${arr[$i]} ]; then
            arr[$i]='""'
        else
            arr[$i]="${arr[$i]}"
        fi
    done

    echo "[$(join_arr ", " ${arr[@]})]"
}

setup()
{
    local sub_setup=$1

    # Create a test directory and `cd` into it
    testdir=$PWD/$SUITE-$test
    mkdir -p "$testdir"
    cd "$testdir"

    # Create test database's name
    testdb=$SUITE-$test

    eval "$sub_setup"
}

teardown()
{
    local sub_teardown=$1

    mongo "$testdb" --eval "db.dropDatabase()" >/dev/null
    rm -rf "$testdir"

    eval "$sub_teardown"
}

error()
{
    echo "$*"
    exit 1
}

run_tests()
{
    local fail=0
    local sub_setup="$1"
    local sub_teardown="$2"
    shift 2

    for test in "$@"; do
        (set -e; trap -- "teardown $sub_teardown" EXIT;
         setup "$sub_setup"; "$test")
        if !(($?)); then
            echo "$test: ✔"
        else
            echo "$test: ✖"
            fail=1
        fi
    done

    return $fail
}
