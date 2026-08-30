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

test_sync_2_files()
{
    truncate -s 1k "fileA"

    rbh_sync_posix "." "rbh:$db:$testdb"
    find_attribute '"ns.xattrs.path":"/"'
    find_attribute '"ns.xattrs.path":"/fileA"'
}

test_sync_size()
{
    truncate -s 1025 "fileA"
    local length=$(stat -c %s "fileA")

    rbh_sync_posix "." "rbh:$db:$testdb"
    find_attribute '"ns.xattrs.path":"/fileA"' '"statx.size" : '$length
}

test_sync_3_files()
{
    truncate -s 1k "fileA"
    truncate -s 1k "fileB"

    rbh_sync_posix "." "rbh:$db:$testdb"
    find_attribute '"ns.xattrs.path":"/"'
    find_attribute '"ns.xattrs.path":"/fileA"'
    find_attribute '"ns.xattrs.path":"/fileB"'
}

test_sync_xattrs()
{
    truncate -s 1k "fileA"
    setfattr -n user.a -v b "fileA"
    truncate -s 1k "fileB"
    setfattr -n user.c -v d "fileB"

    rbh_sync_posix "." "rbh:$db:$testdb"
    find_attribute '"ns.xattrs.path":"/fileA"' \
                   '"xattrs.user.a" : { $exists : true }'
    find_attribute '"ns.xattrs.path":"/fileB"' \
                   '"xattrs.user.c" : { $exists : true }'
}

test_sync_subdir()
{
    mkdir "dir"
    truncate -s 1k "dir/file"
    truncate -s 1k "fileA"
    truncate -s 1k "fileB"

    rbh_sync_posix "." "rbh:$db:$testdb"
    find_attribute '"ns.xattrs.path":"/"'
    find_attribute '"ns.xattrs.path":"/fileA"'
    find_attribute '"ns.xattrs.path":"/fileB"'
    find_attribute '"ns.xattrs.path":"/dir"'
    find_attribute '"ns.xattrs.path":"/dir/file"'
}

test_sync_large_tree()
{
    mkdir -p {1..9}/{1..9}

    rbh_sync_posix "." "rbh:$db:$testdb"
    for i in $(find *); do
        find_attribute '"ns.xattrs.path":"/'$i'"'
    done
}

test_sync_one_one_file()
{
    truncate -s 1k "fileA"
    local length=$(stat -c %s "fileA")

    rbh_sync_posix_one "fileA" "rbh:$db:$testdb"
    find_attribute '"statx.size" : '$length
}

check_mode_and_type()
{
    local entry="$1"

    local raw_mode="$(stat -c %f "$entry")"
    raw_mode=$(echo "ibase=16; ${raw_mode^^}" | bc)
    local type=$((raw_mode & 00170000))
    local mode=$((raw_mode & ~00170000))

    find_attribute '"statx.type":'$type
    find_attribute '"statx.mode":'$mode
}

test_sync_one()
{
    mkdir "dir"
    touch "dir/file"
    ln -s "dir/file" "file_link"

    rbh_sync_posix_one "file_link" "rbh:$db:$testdb"
    check_mode_and_type "file_link"
    rbh_sync_posix_one "dir" "rbh:$db:$testdb"
    rbh_sync_posix_one "dir/file" "rbh:$db:$testdb"

    find_attribute '"ns.name":"file_link"'
    find_attribute '"ns.xattrs.path":"/file_link"'
    find_attribute '"ns.name":"dir"'
    find_attribute '"ns.xattrs.path":"/dir"'
    find_attribute '"ns.name":"file"'
    find_attribute '"ns.xattrs.path":"/dir/file"'
}

test_sync_one_two_files()
{
    truncate -s 1k "fileA"
    truncate -s 1k "fileB"
    setfattr -n user.a -v b "fileB"
    local length=$(stat -c %s "fileB")

    rbh_sync_posix_one "fileA" "rbh:$db:$testdb"
    rbh_sync_posix_one "fileB" "rbh:$db:$testdb"
    find_attribute '"statx.size" : '$length \
                   '"xattrs.user.a" : { $exists : true }'

    local entries=$(count_documents)
    if [[ $entries -ne 2 ]]; then
        error "Invalid number of files were synced, expected '2', " \
              "found '$entries'."
    fi
}

test_sync_symbolic_link()
{
    local entry="symbolic_link"

    touch ${entry}_target
    ln -s ${entry}_target $entry

    rbh_sync -o "rbh:posix:$entry" "rbh:$db:$testdb"
    check_mode_and_type $entry
}

test_sync_socket()
{
    local entry="socket_file"

    python3 -c "import socket as s; \
                sock = s.socket(s.AF_UNIX); \
                sock.bind('$entry')"

    rbh_sync_posix_one "$entry" "rbh:$db:$testdb"
    check_mode_and_type $entry
}

test_sync_fifo()
{
    local entry="fifo_file"

    mkfifo $entry

    rbh_sync_posix_one "$entry" "rbh:$db:$testdb"
    check_mode_and_type $entry
}

test_sync_branch()
{
    local first_dir="test1"
    local second_dir="test2"
    local third_dir="test3"
    local entry="random_file"

    mkdir -p $first_dir/$second_dir/$third_dir
    touch $first_dir/$second_dir/$third_dir/$entry

    rbh_sync_posix "$first_dir#$second_dir" "rbh:$db:$testdb"

    find_attribute '"ns.name":"'$second_dir'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir'"'
    find_attribute '"ns.name":"'$third_dir'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir/$third_dir'"'
    find_attribute '"ns.name":"'$entry'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir/$third_dir/$entry'"'

    do_db drop $testdb

    local abs_path="$(realpath $first_dir)"

    rbh_sync_posix "$abs_path#$second_dir" "rbh:$db:$testdb"

    find_attribute '"ns.name":"'$second_dir'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir'"'
    find_attribute '"ns.name":"'$third_dir'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir/$third_dir'"'
    find_attribute '"ns.name":"'$entry'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir/$third_dir/$entry'"'

    do_db drop $testdb

    rbh_sync_posix "$first_dir#$second_dir/$third_dir" "rbh:$db:$testdb"

    find_attribute '"ns.name":"'$third_dir'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir/$third_dir'"'
    find_attribute '"ns.name":"'$entry'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir/$third_dir/$entry'"'

    do_db drop $testdb

    rbh_sync_posix "./$first_dir#$second_dir/$third_dir" "rbh:$db:$testdb"

    find_attribute '"ns.name":"'$third_dir'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir/$third_dir'"'
    find_attribute '"ns.name":"'$entry'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir/$third_dir/$entry'"'

    do_db drop $testdb

    rbh_sync_posix "$first_dir/../$first_dir/./$second_dir#$third_dir" \
                   "rbh:$db:$testdb"

    find_attribute '"ns.name":"'$third_dir'"'
    find_attribute '"ns.xattrs.path":"'/$third_dir'"'
    find_attribute '"ns.name":"'$entry'"'
    find_attribute '"ns.xattrs.path":"'/$third_dir/$entry'"'
}

test_sync_large_path()
{
    # We will create strings of length 4064 and 4096 by creating a file
    # hierarchy, with an additionnal 32 '/'
    local root_len=$(realpath . | wc -c)
    local path_len=$((4096 - $root_len - 32))

    local lengthA=$(($path_len / 32))
    local lengthB=$((($path_len / 32) + 1))

    local long_pathA="$(printf '%*s' "$lengthA" | sed 's/ /a/g')"
    local long_pathB="$(printf '%*s' "$lengthB" | sed 's/ /b/g')"

    local full_pathA
    local full_pathB

    for i in $(seq 1 32);
    do
        full_pathA="$full_pathA/$long_pathA"
        full_pathB="$full_pathB/$long_pathB"
    done

    mkdir -p ./$full_pathA
    mkdir -p ./$full_pathB

    # The path A should be synced properly, as its size is 4064 characters
    # total, but the path B shouldn't be, as it exceeds the path size limit
    rbh_sync_posix "." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path":"'$full_pathA'"'
    ! (find_attribute '"ns.xattrs.path":"'$full_pathB'"')
}

test_sync_dir_delete_while_mfu_walk()
{
    mkdir dir
    touch dir/fileA
    touch fileB

    tmp_gdb_script=$(mktemp)
    cat << EOF > "$tmp_gdb_script"
set breakpoint pending on
break rbh_iter_chunkify
commands
shell rm -rf dir
continue
end
run
EOF

    DEBUGINFOD_URLS="" gdb --batch -x "$tmp_gdb_script" \
        --args $__rbh_sync "rbh:posix-mpi:." "rbh:$db:$testdb"

    find_attribute '"ns.xattrs.path": "/"' \
                   '"xattrs.nb_children.value": 1'
    find_attribute '"ns.xattrs.path": "/fileB"'
    ! (find_attribute '"ns.xattrs.path": "/dir"')
}

test_stats()
{
    mkdir -p {1..9}/{1..9}

    local output="$(rbh_sync_posix . rbh:$db:$testdb --stats)"

    echo "$output" | grep "rbh-sync" > /dev/null ||
        error "Should have found 'rbh-sync' mentionned, got '$output'"

    # 9 * 9 + 9 directories + root = 91 entries
    echo "$output" | grep "progress" | grep "91" > /dev/null ||
        error "Last log shown should have found 91 entries in total, got '$output'"

    echo "$output" | grep "current speed" | grep "entries/sec" > /dev/null ||
        error "Logs should show the current speed in entries per second, got '$output'"

    rbh_sync_posix . rbh:$db:$testdb --stats --log-file logs.txt
    output="$(cat logs.txt)"

    echo "$output" | grep "rbh-sync" > /dev/null ||
        error "Should have found 'rbh-sync' mentionned, got '$output'"

    # 9 * 9 + 9 directories + root + logs.txt = 92 entries
    echo "$output" | grep "progress" | grep "92" > /dev/null ||
        error "Last log shown should have found 92 entries in total, got '$output'"

    echo "$output" | grep "current speed" | grep "entries/sec" > /dev/null ||
        error "Logs should show the current speed in entries per second, got '$output'"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_sync_2_files test_sync_size test_sync_3_files
                  test_sync_xattrs test_sync_subdir test_sync_large_tree
                  test_sync_one_one_file test_sync_one test_sync_one_two_files
                  test_sync_symbolic_link test_sync_socket test_sync_fifo
                  test_sync_branch)

if [[ $WITH_MPI == true ]]; then
    tests+=(test_sync_large_path test_sync_dir_delete_while_mfu_walk)
else
    tests+=(test_stats)
fi

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
add_test_user $test_user
trap -- "rm -rf '$tmpdir'; delete_test_user $test_user" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
