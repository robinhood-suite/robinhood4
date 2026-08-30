#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash
. $test_dir/posix_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_continue_sync_on_error()
{
    local first_file="test1"
    local second_file="test2"
    local third_file="test3"
    local dir="dir"

    mkdir $dir
    touch $first_file $second_file $dir/$third_file
    chmod o-rw $second_file
    chmod o-rw $dir

    # Here, we run a rbh-sync on the files created above as a fake user. Since
    # that user doesn't have the read or write access to the second file and
    # the directory, it cannot synchronize both, the command should fail when
    # synchronizing the second file and the directory

    local output=$(sync_with_other_user)

    echo "$output" | grep "open '/$second_file'" ||
        error "Failed to find error on open of '$second_file'"
    echo "$output" | grep "open '/$dir'" ||
        error "Failed to find error on open of '$dir'"

    if [[ "$WITH_MPI" == "true" ]]; then
        # We check if there is an error from mpifileutils while opening /dir
        local realpath=$(realpath $dir)
        echo "$output" | \
        grep "ERROR: Failed to open directory with opendir: '$realpath'" || \
        error "Failed to find error on open of '$dir'"
    else
        echo "$output" | grep "FTS" | grep "read entry './$dir'" ||
            error "Failed to find error on open of '$dir'"
    fi

    local db_count=$(count_documents)
    if [[ $db_count -ne 2 ]]; then
        error "Invalid number of files were synced, expected '2' entries, " \
              "found '$db_count'."
    fi

    find_attribute '"ns.xattrs.path":"/"'
    find_attribute '"ns.xattrs.path":"/"' '"xattrs.nb_children.value": 1'
    find_attribute '"ns.name":"'$first_file'"'
}

test_stop_sync_on_error()
{
    local first_file="test1"
    local second_file="test2"
    local third_file="test3"
    local dir="dir"

    mongo_only_test

    touch $first_file
    touch $second_file
    mkdir $dir
    touch $dir/$third_file

    chmod o-rw $second_file
    chmod o-rw $dir

    # Here, we run a rbh-sync on the files created above as a fake user. Since
    # that user doesn't have the read or write access to the second file and
    # the directory, it cannot synchronize both, the command should fail when
    # synchronizing the second file and the directory

    local output=$(sync_with_other_user "--no-skip")

    local db_count=$(count_documents)
    if [[ $db_count -ne 0 ]]; then
        error "Invalid number of files were synced, expected '0', found" \
              "'$db_count' entries."
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_continue_sync_on_error test_stop_sync_on_error)

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
add_test_user $test_user
trap -- "rm -rf '$tmpdir'; delete_test_user $test_user" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
