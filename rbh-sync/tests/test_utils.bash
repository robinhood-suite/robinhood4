#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

################################################################################
#                                  UTILITIES                                   #
################################################################################

SUITE=${BASH_SOURCE##*/}
SUITE=${SUITE%.*}

__rbh_sync=$(PATH="$PWD:$PATH" which rbh-sync)
rbh_sync()
{
    if [[ "$WITH_MPI" == "true" ]]; then
        mpirun --allow-run-as-root -np 4 "$__rbh_sync" "$@"
    else
        "$__rbh_sync" "$@"
    fi
}

__mongo=$(which mongosh || which mongo)
mongo()
{
    "$__mongo" --quiet "$@"
}

setup()
{
    # Create a test directory and `cd` into it
    testdir=$PWD/$SUITE-$test
    mkdir "$testdir"
    cd "$testdir"

    # Create test database's name
    testdb=$SUITE-$test

    # Load MPI
    if [[ "$WITH_MPI" == "true" ]]; then
        module load mpi/openmpi-x86_64
    fi
}

teardown()
{
    mongo "$testdb" --eval "db.dropDatabase()" >/dev/null
    rm -rf "$testdir"
    if [ "$(type -t $test_teardown)" == "function" ]; then
        $test_teardown
    fi
}

error()
{
    echo "$*"
    exit 1
}

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
        arr[$i]="${arr[$i]}"
    done

    echo "[$(join_arr ", " ${arr[@]})]"
}

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
