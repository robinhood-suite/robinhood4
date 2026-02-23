#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_sort()
{
    touch file1 file2 file3

    local inode_to_path=(
        "$(stat -c %i file1) /file1"
        "$(stat -c %i file2) /file2"
    )
    IFS=$'\n' inode_to_path_sorted=($(sort -n <<<"${inode_to_path[*]}"))
    unset IFS

    local ino=(
        $(echo "${inode_to_path_sorted[0]}" | cut -d' ' -f1)
        $(echo "${inode_to_path_sorted[1]}" | cut -d' ' -f1)
    )

    local path=(
        $(echo "${inode_to_path_sorted[0]}" | cut -d' ' -f2)
        $(echo "${inode_to_path_sorted[1]}" | cut -d' ' -f2)
    )

    echo "blob" > file1
    echo "blob" > file2
    echo "something" > file3

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -sort size |
        difflines "/file1" "/file2" "/file3" "/"
    rbh_find "rbh:$db:$testdb" -rsort size |
        difflines "/" "/file3" "/file1" "/file2"

    rbh_find "rbh:$db:$testdb" -sort size -sort ino |
        difflines "${path[0]}" "${path[1]}" "/file3" "/"
    rbh_find "rbh:$db:$testdb" -sort size -rsort ino |
        difflines "${path[1]}" "${path[0]}" "/file3" "/"

    rbh_find "rbh:$db:$testdb" -rsort size -sort ino |
        difflines "/" "/file3" "${path[0]}" "${path[1]}"
    rbh_find "rbh:$db:$testdb" -rsort size -rsort ino |
        difflines "/" "/file3" "${path[1]}" "${path[0]}"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_sort)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'"  EXIT
cd "$tmpdir"

run_tests ${tests[@]}
