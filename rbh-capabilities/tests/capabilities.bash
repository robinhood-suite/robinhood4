#!/usr/bin/env bash

# This file is part of rbh-capabilities.
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later
test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash
################################################################################
#                                    TESTS                                     #
################################################################################
tests_backend_installed_list()
{
    local output=$(rbh-capabilities --list)
    echo "$output" | grep -q "mongo"
    echo "$output" | grep -q "posix"
    echo "$output" | grep -q "List"
}

tests_mongo_capabilities()
{
    local output=$(rbh-capabilities mongo)
    echo "$output" | grep -q "update"
    echo "$output" | grep -q "filter"
    echo "$output" | grep -q "synchronisation"
    echo "$output" | grep -q "branch"
}

tests_posix_capabilities()
{
    local output=$(rbh-capabilities posix)
    echo "$output" | grep -q "synchronisation"
    echo "$output" | grep -q "branch"
}

tests_not_installed_capabilities()
{
    rbh-capabilities not_a_backend &&
        error "Expected capabilities with an unknown backend to fail"

    return 0
}

tests_library_path_env_not_exist()
{
    if [ ! -z "$LD_LIBRARY_PATH" ]; then
        local TEMP_VAR=$LD_LIBRARY_PATH
        unset LD_LIBRARY_PATH
        tests_backend_installed_list
        export LD_LIBRARY_PATH=$TEMP_VAR
        unset TEMP_VAR
    else
        tests_backend_installed_list
    fi
}

tests_library_path_env_invalid()
{
    if [ ! -z "$LD_LIBRARY_PATH" ]; then
        local TEMP_VAR=$LD_LIBRARY_PATH
    fi

    export LD_LIBRARY_PATH="/home/username/none:/tmp/none:/usr"
    tests_backend_installed_list
    export LD_LIBRARY_PATH="text.txt"
    tests_backend_installed_list

    if [ ! -z "$TEMP_VAR" ]; then
        export LD_LIBRARY_PATH=$TEMP_VAR
        unset TEMP_VAR
    fi
}
################################################################################
#                                     MAIN                                     #
################################################################################
declare -a tests=(tests_backend_installed_list tests_mongo_capabilities
                  tests_posix_capabilities tests_not_installed_capabilities
                  tests_library_path_env_not_exist
                  tests_library_path_env_invalid)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"
run_tests ${tests[@]}
