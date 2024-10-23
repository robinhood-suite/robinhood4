#!/usr/bin/env bash

# This file is part of rbh-capabilities.
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

tests_backend_installed_list()
{
    local output=$(rbh_capabilities --list)
    echo "$output" | grep -q "mongo"
    echo "$output" | grep -q "posix"
    echo "$output" | grep -q "List"
}

tests_mongo_capabilities()
{
    local output=$(rbh_capabilities mongo)
    echo "$output" | grep -q "update"
    echo "$output" | grep -q "filter"
    echo "$output" | grep -q "synchronisation"
    echo "$output" | grep -q "branch"
}

tests_posix_capabilities()
{
    local output=$(rbh_capabilities posix)
    echo "$output" | grep -q "synchronisation"
    echo "$output" | grep -q "branch"
}

tests_not_find_backend_list()
{
    (rbh_capabilities -l | grep -q "find") &&
        error "rbh-capabilities -l must not return anything other than backends"

    return 0
}

tests_not_installed_capabilities()
{
    rbh_capabilities not_a_backend &&
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

tests_library_path_correctly_set()
{
    export LD_LIBRARY_PATH="${LD_LIBRARY_PATH}:/usr/lib64"

    local output=$(rbh_capabilities mongo)
    if echo "$output" | grep -q "Capabilities of mongo:"; then
        echo "Mongo backend imported successfully as expected"
    else
        error "Mongo backend import failed: Expected 'Capabilities of mongo:'
            but got: $output"
    fi
}

tests_mongo_backend_detection()
{
    local backend_path="/home/$SUDO_USER/robinhood4/builddir/librobinhood/"\
"src/backends/mongo"
    local non_standard_path="/tmp/non_standard"
    local TEMP=$LD_LIBRARY_PATH

    mkdir -p $non_standard_path

    test -f /usr/lib/librbh-mongo.so &&
        mv /usr/lib/librbh-mongo.so /tmp/librbh-mongo.so.lib
    test -f /usr/lib/librbh-mongo.so.1 &&
        mv /usr/lib/librbh-mongo.so.1 /tmp/librbh-mongo.so.1.lib
    test -f /usr/lib64/librbh-mongo.so &&
        mv /usr/lib64/librbh-mongo.so /tmp/librbh-mongo.so.lib64
    test -f /usr/lib64/librbh-mongo.so.1 &&
        mv /usr/lib64/librbh-mongo.so.1 /tmp/librbh-mongo.so.1.lib64

    mv $backend_path/librbh-mongo.so $non_standard_path
    cp $backend_path/librbh-mongo.so.1 $non_standard_path/

    unset LD_LIBRARY_PATH
    local output=$(rbh_capabilities mongo 2>&1)

    if echo "$output" | grep -q "This backend does not exist"; then
        echo "Mongo backend correctly not imported as expected"
    else
        error "Mongo backend was unexpectedly detected in default paths"
    fi

    export LD_LIBRARY_PATH=$non_standard_path
    output=$(rbh_capabilities mongo 2>&1)
    mv $non_standard_path/librbh-mongo.so $backend_path

    if echo "$output" | grep -q "Capabilities of mongo"; then
        echo "Mongo backend imported successfully from non-standard path"
    else
        error "Mongo backend import failed from non-standard path: $output"
        rm -rf $non_standard_path
    fi

    test -f /tmp/librbh-mongo.so.lib &&
        mv /tmp/librbh-mongo.so.lib /usr/lib/librbh-mongo.so
    test -f /tmp/librbh-mongo.so.1.lib &&
        mv /tmp/librbh-mongo.so.1.lib /usr/lib/librbh-mongo.so.1
    test -f /tmp/librbh-mongo.so.lib64 &&
        mv /tmp/librbh-mongo.so.lib64 /usr/lib64/librbh-mongo.so
    test -f /tmp/librbh-mongo.so.1.lib64 &&
        mv /tmp/librbh-mongo.so.1.lib64 /usr/lib64/librbh-mongo.so.1

    export LD_LIBRARY_PATH=$TEMP
    rm -rf $non_standard_path
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(tests_backend_installed_list tests_mongo_capabilities
                  tests_posix_capabilities tests_not_installed_capabilities
                  tests_not_find_backend_list tests_library_path_correctly_set
                  tests_mongo_backend_detection)
                  # tests_library_path_env_not_exist
                  # tests_library_path_env_invalid)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
