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

MESON_BUILDDIR=$PWD

# The default OFI BTL has a hard coded 1s sleep on MPI_Finalize(). Since
# we do a lot of small MPI jobs, this has a huge impact on the test
# duration. The self BTL does not have this sleep.
export OMPI_MCA_btl=self

# Some tests rely on the output of date and the sorting order of sort.
# Both of these are local dependant. So we fix the local to C.
export LC_ALL=C

# BASH_SOURCE[0] is the relative path to this file.
export RBH_CONFIG_PATH="$(dirname ${BASH_SOURCE[0]})/test_config.yaml"

__db_cmd_path="$(dirname ${BASH_SOURCE[0]})"

__rbh_sync=$(PATH="$PWD/rbh-sync:$PATH" which rbh-sync)
rbh_sync()
{
    if [[ "$WITH_MPI" == "true" ]]; then
        mpirun --allow-run-as-root "$__rbh_sync" "$@"
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

__rbh_info=$(PATH="$PWD/rbh-info:$PATH" which rbh-info)
rbh_info()
{
    "$__rbh_info" "$@"
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

join_arr()
{
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

do_db()
{
    "$__db_cmd_path/db_cmd_$db" "$@"
}

count_documents()
{
    do_db count "$testdb"
}

find_attribute()
{
    do_db find "$testdb" "$@"
}

difflines()
{
    diff -y - <([ $# -eq 0 ] && printf '' || printf '%s\n' "$@")
}

archive_file()
{
    local file="$1"

    sudo lfs hsm_archive "$file"

    while ! lfs hsm_state "$file" | awk -F')' '{print $2}' | grep "archived"; do
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
    # Execute the full teardown regardless of errors otherwise, things aren't
    # properly cleaned.
    set +e

    local output=$(do_db drop "$testdb" 2>&1)
    if (( $? != 0 )); then
        echo "Failed to drop '$testdb':"
        echo "$output"
    fi

    rm -rf "$testdir"
    if [ "$(type -t $test_teardown)" == "function" ]; then
        $test_teardown
    fi

    eval "$sub_teardown"
}

run_tests()
{
    local fail=0
    local timefile=$(mktemp)

    if [[ "$WITH_MPI" != "true" ]]; then
        db=${RBH_TEST_DB:-mongo}
    else
        # SQLite does not support concurrent writers so use mongo for mpi
        # tests.
        db=mongo
    fi

    for test in "$@"; do
        (
         set -e
         trap -- "teardown $sub_teardown" EXIT

         # The time needs to be stored in a file because we can't propagate bash
         # or env variable outside of this subshell.
         date +%s > "$timefile"

         setup "$sub_setup"
         "$test"
        )
        local rc=$?
        local end=$(date +%s)
        local duration=$(( $end - $(cat "$timefile") ))

        if (( rc == 0 )); then
            echo "$test: ✔  (${duration}s)"
        elif (( rc == 77 )); then
            echo "$test: SKIP"
            if (( fail == 0 )); then
                fail="77"
            fi
        else
            echo "$test: ✖  (${duration}s)"
            fail=$rc
        fi
    done

    rm "$timefile"

    return $fail
}
