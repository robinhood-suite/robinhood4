#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_inum()
{
    touch file1 file2 file3

    local inode_to_path=(
        "$(stat -c %i .) /"
        "$(stat -c %i file1) /file1"
        "$(stat -c %i file2) /file2"
        "$(stat -c %i file3) /file3"
    )
    IFS=$'\n' inode_to_path_sorted=($(sort -n <<<"${inode_to_path[*]}"))
    unset IFS

    local ino=(
        $(echo "${inode_to_path_sorted[0]}" | cut -d' ' -f1)
        $(echo "${inode_to_path_sorted[1]}" | cut -d' ' -f1)
        $(echo "${inode_to_path_sorted[2]}" | cut -d' ' -f1)
        $(echo "${inode_to_path_sorted[3]}" | cut -d' ' -f1)
    )

    local path=(
        $(echo "${inode_to_path_sorted[0]}" | cut -d' ' -f2)
        $(echo "${inode_to_path_sorted[1]}" | cut -d' ' -f2)
        $(echo "${inode_to_path_sorted[2]}" | cut -d' ' -f2)
        $(echo "${inode_to_path_sorted[3]}" | cut -d' ' -f2)
    )

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -inum ${ino[1]} -sort ino |
        difflines "${path[1]}"
    rbh_find "rbh:mongo:$testdb" -inum +${ino[1]} -sort ino |
        difflines "${path[2]}" "${path[3]}"
    rbh_find "rbh:mongo:$testdb" -inum -${ino[2]} -sort ino |
        difflines "${path[0]}" "${path[1]}"

    rbh_find "rbh:mongo:$testdb" -inum ${ino[0]} -o -inum +${ino[2]} -sort ino |
        difflines "${path[0]}" "${path[3]}"
    rbh_find "rbh:mongo:$testdb" -inum +${ino[2]} -a -inum -${ino[2]} -sort ino |
        difflines
    rbh_find "rbh:mongo:$testdb" -not -inum ${ino[2]} -sort ino |
        difflines "${path[0]}" "${path[1]}" "${path[3]}"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_inum)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'"  EXIT
cd "$tmpdir"

run_tests ${tests[@]}
