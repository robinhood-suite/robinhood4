#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

################################################################################
#                                  UTILITIES                                   #
################################################################################

# Use BASH_SOURCE[1] as this file will always be sourced by the actual test file
SUITE=${BASH_SOURCE[1]##*/}
SUITE=${SUITE%.*}

__rbh_sync=$(PATH="$PWD/rbh-sync:$PATH" which rbh-sync)
rbh_sync()
{
    if [[ "$WITH_MPI" == "true" ]]; then
        mpirun --allow-run-as-root -np 4 "$__rbh_sync" "$@"
    else
        "$__rbh_sync" "$@"
    fi
}

__rbh_find=$(PATH="$PWD/rbh-find:$PATH" which rbh-find)
rbh_find()
{
    "$__rbh_find" "$@"
}

__rbh_report=$(PATH="$PWD/rbh-report:$PATH" which rbh-report)
rbh_report()
{
    "$__rbh_report" "$@"
}

__rbh_fsevents=$(PATH="$PWD/rbh-fsevents:$PATH" which rbh-fsevents)
rbh_fsevents()
{
    "$__rbh_fsevents" "$@"
}

__rbh_lfind=$(PATH="$PWD/rbh-find-lustre:$PATH" which rbh-lfind)
rbh_lfind()
{
    "$__rbh_lfind" "$@"
}

__rbh_capabilities=$(PATH="$PWD/rbh-capabilities:$PATH" which rbh-capabilities)
rbh_capabilities()
{
    "$__rbh_capabilities" "$@"
}

__rbh_update_retention=$(PATH="$PWD/../retention:$PATH" which rbh_update_retention)
__update_retention_PATH="$PWD/rbh-find-lustre:"
__update_retention_PATH+="$PWD/rbh-sync:"
__update_retention_PATH+="$PWD/rbh-fsevents:"
__update_retention_PATH+="$PWD/rbh-find"

rbh_update_retention()
{
    PATH="$__update_retention_PATH:$PATH" "$__rbh_update_retention" "$@"
}

__mongo=$(which mongosh || which mongo)
mongo()
{
    "$__mongo" --quiet "$@"
}

function version_code()
{
    eval set -- $(echo $1 | tr "[:punct:]" " ")

    echo -n "$(( (${1:-0} << 16) | (${2:-0} << 8) | ${3:-0} ))"
}

function mongo_version()
{
    local version="$(mongod --version | head -n 1 | cut -d' ' -f3)"
    version_code "${version:1}"
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
        arr[$i]="NumberLong(\"${arr[$i]}\")"
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

count_documents()
{
    local input="$1"

    if [ ! -z "$input" ]; then
        input="{$input}"
    fi

    if (( $(mongo_version) < $(version_code 5.0.0) )); then
        mongo $testdb --eval "db.entries.count($input)"
    else
        mongo $testdb --eval "db.entries.countDocuments($input)"
    fi
}

find_attribute()
{
    old_IFS=$IFS
    IFS=','
    local output="$*"
    IFS=$old_IFS
    local res="$(count_documents "$output")"
    [[ "$res" == "1" ]] && return 0 ||
        error "No entry found with filter '$output'"
}

difflines()
{
    diff -y - <([ $# -eq 0 ] && printf '' || printf '%s\n' "$@")
}

archive_file()
{
    local file="$1"

    sudo lfs hsm_archive "$file"

    while ! lfs hsm_state "$file" | grep "archive_id:"; do
        sleep 0.5
    done
}

get_test_user()
{
    local test_name="$1"

    local user_name="user_$test_name"
    echo "${user_name:0:31}"
}

add_test_user()
{
    local test_user="$1"

    useradd -K MAIL_DIR=/dev/null -lMN $test_user
}

delete_test_user()
{
    local test_user="$1"

    userdel $test_user
}

################################################################################
#                                  TESTS FRAMEWORK                             #
################################################################################

setup()
{
    local sub_setup="$1"

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

    eval "$sub_setup"
}

teardown()
{
    local sub_teardown="$1"

    mongo "$testdb" --eval "db.dropDatabase()" >/dev/null
    rm -rf "$testdir"
    if [ "$(type -t $test_teardown)" == "function" ]; then
        $test_teardown
    fi

    eval "$sub_teardown"
}

run_tests()
{
    local fail=0

    for test in "$@"; do
        (
         set -e
         trap -- "teardown $sub_teardown" EXIT
         setup "$sub_setup"
         "$test"
        )
        rc=$?
        echo "rc = '$rc'"
        if ((rc == 77)); then
            echo "$test: SKIP"
            if ((fail == 0)); then
                fail="77"
            fi
        elif !((rc)); then
            echo "$test: ✔"
        else
            echo "$test: ✖"
            if ((fail == 0 || fail == 77)); then
                fail="$rc"
            fi
        fi
    done

    return $fail
}
