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

test_sync_number_children()
{
    mkdir -p dir1/dir2/dir3
    touch dir1/fileA dir1/fileB dir1/dir2/fileC

    # dir_locked and file_locked shouldn't be counted
    mkdir dir_locked
    touch dir_locked/fileD
    touch file_locked

    chmod o-rw dir_locked
    chmod o-rw file_locked

    local output=$(sync_with_other_user)

    find_attribute '"ns.xattrs.path": "/"' '"xattrs.nb_children.value": 1'
    find_attribute '"ns.xattrs.path": "/dir1"' '"xattrs.nb_children.value": 3'
    find_attribute '"ns.xattrs.path": "/dir1/dir2"' '"xattrs.nb_children.value": 2'
    find_attribute '"ns.xattrs.path": "/dir1/dir2/dir3"'\
                   '"xattrs.nb_children.value": 0'
    ! (find_attribute '"ns.xattrs.path": "/dir1/fileA"' \
                      '"xattrs.nb_children": {$exists: true}')
}

test_sync_number_children_mpi()
{
    mkdir -p root/dir{1..2}/dir{1..2}/dir{1..2}

    touch root/file{1..10}
    touch root/dir{1..2}/file{1..10}
    touch root/dir{1..2}/dir{1..2}/file{1..10}
    touch root/dir{1..2}/dir{1..2}/dir{1..2}/file{1..12}

    rbh_sync_posix "root" "rbh:$db:$testdb"

    local expected_children=12
    local directories=($(find root -type d | xargs))

    local expected_nb_directories=${#directories[@]}
    local nb_directories=$(count_documents '"xattrs.nb_children.value": {$gt: 0}')

    if [[ $expected_nb_directories != $nb_directories ]]; then
        error "There should be $expected_nb_directories with a number of" \
              "children greater than 0, got $nb_directories"
    fi

    for dir in ${directories[@]}; do
        local name=${dir#"root"}
        if [[ -z "$name" ]]; then
            name="/"
        fi

        find_attribute '"ns.xattrs.path": "'"$name"'"'\
                       '"xattrs.nb_children.value": '"$expected_children"''
    done
}

test_nb_children_two_sync()
{
    mkdir test
    touch test/{1..5}
    mkdir test/dir

    rbh_sync_posix "." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path": "/test"' '"xattrs.nb_children.value": 6'
    find_attribute '"ns.xattrs.path": "/test/dir"' \
        '"xattrs.nb_children.value": 0'

    rbh_sync_posix "." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path": "/test"' '"xattrs.nb_children.value": 6'
    find_attribute '"ns.xattrs.path": "/test/dir"' \
        '"xattrs.nb_children.value": 0'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_sync_number_children test_nb_children_two_sync)

if [[ $WITH_MPI == true ]]; then
    tests+=(test_sync_number_children_mpi)
fi

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
add_test_user $test_user
trap -- "rm -rf '$tmpdir'; delete_test_user $test_user" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
