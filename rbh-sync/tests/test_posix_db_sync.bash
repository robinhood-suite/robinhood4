#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

# Depending on the libfabric's version and OS, libfabric can have network errors
# with PSM3. To solve this, we specify the PSM3 devices as below.
# https://github.com/easybuilders/easybuild-easyconfigs/issues/18925
export PSM3_DEVICES="self"

rbh_sync_posix()
{
    if [[ "$WITH_MPI" == "true" ]]; then
        rbh_sync "rbh:posix-mpi:$1" "$2" $3
    else
        rbh_sync "rbh:posix:$1" "$2" $3
    fi
}

rbh_sync_posix_one()
{
    if [[ "$WITH_MPI" == "true" ]]; then
        rbh_sync -o "rbh:posix-mpi:$1" "$2" $3
    else
        rbh_sync -o "rbh:posix:$1" "$2" $3
    fi
}

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

set_permission()
{
    local path=$1
    local sign=$2

    while [[ "$path" != "/home" ]] && [[ "$path" != "/" ]]; do
        if [[ "$sign" == "+" ]]; then
            chmod o+rx $path
        else
            chmod o-rx $path
        fi
        path="$(dirname $path)"
    done
}

sync_with_other_user()
{
    local skip_option=$1
    local path="$(dirname $__rbh_sync)"
    set_permission $path "+"

    local path_config="$(realpath $RBH_CONFIG_PATH)"
    set_permission $path_config "+"

    if [[ "$WITH_MPI" == "true" ]]; then
        # We need to give execute permissions to the user for mpirun to run
        chmod o+x ..
        local output="$(sudo -H -u "$test_user" bash -c \
                        "source /etc/profile.d/modules.sh; \
                         module load mpi/openmpi-x86_64; \
                         LD_LIBRARY_PATH=$LD_LIBRARY_PATH \
                         RBH_CONFIG_PATH=$RBH_CONFIG_PATH \
                         mpirun $__rbh_sync --config $path_config $skip_option \
                         rbh:posix-mpi:. rbh:$db:$testdb" 2>&1)"
    else
        local output="$(sudo -E -H -u "$test_user" bash -c "\
                        LD_LIBRARY_PATH=$LD_LIBRARY_PATH \
                        $__rbh_sync --config $path_config $skip_option \
                        rbh:posix:. rbh:$db:$testdb" 2>&1)"
    fi

    set_permission $path "-"

    set_permission $path_config "-"

    echo "$output"
}

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
    find_attribute '"ns.xattrs.path":"/"' '"xattrs.nb_children": 1'
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

test_config()
{
    local conf_file="conf"
    local file="test_file"

    mongo_only_test

    touch $file

    echo "---
 RBH_MONGODB_ADDRESS: \"mongodb://localhost:27017\"
---" > $conf_file

    rbh_sync --config $conf_file --one rbh:posix:$file rbh:$db:$testdb

    find_attribute '"ns.xattrs.path":"/'$file'"'

    echo "---
 RBH_MONGODB_ADDRESS: \"mongodb://localhost:12345\"
---" > $conf_file

    rbh_sync --config $conf_file --one rbh:posix:$file rbh:$db:$testdb &&
        error "Sync with invalid server address in config should have failed"

    echo "---
 RBH_MONGODB_ADDRESS: !int32 12345
---" > $conf_file

    rbh_sync --config $conf_file --one rbh:posix:$file rbh:$db:$testdb &&
        error "Sync with invalid typing for db address in config should have" \
              "failed"

    echo "---
xattrs_map:
    user.blob_int32: int32
    user.blob_int64: int64
    user.blob_uint32: unsigned int32
    user.blob_uint64: unsigned int64
    user.blob_string: string
    user.blob_boolean: boolean
---" > $conf_file

    setfattr -n user.blob_int32 -v 1 $file
    setfattr -n user.blob_int64 -v 2 $file
    setfattr -n user.blob_uint32 -v 3 $file
    setfattr -n user.blob_uint64 -v 4 $file
    setfattr -n user.blob_string -v five $file
    setfattr -n user.blob_boolean -v true $file

    rbh_sync --config $conf_file --one rbh:posix:$file rbh:$db:$testdb

    find_attribute '"ns.xattrs.path":"/'$file'"' \
                   '"xattrs.user.blob_int32" : 1'
    find_attribute '"ns.xattrs.path":"/'$file'"' \
                   '"xattrs.user.blob_int64" : 2'
    find_attribute '"ns.xattrs.path":"/'$file'"' \
                   '"xattrs.user.blob_uint32" : 3'
    find_attribute '"ns.xattrs.path":"/'$file'"' \
                   '"xattrs.user.blob_uint64" : 4'
    find_attribute '"ns.xattrs.path":"/'$file'"' \
                   '"xattrs.user.blob_string" : "five"'
    find_attribute '"ns.xattrs.path":"/'$file'"' \
                   '"xattrs.user.blob_boolean" : true'
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

test_sync_number_children()
{
    # FIXME remove when nb_children is implemented in SQLite backend
    mongo_only_test

    mkdir -p dir1/dir2/dir3
    touch dir1/fileA dir1/fileB dir1/dir2/fileC

    # dir_locked and file_locked shouldn't be counted
    mkdir dir_locked
    touch dir_locked/fileD
    touch file_locked

    chmod o-rw dir_locked
    chmod o-rw file_locked

    local output=$(sync_with_other_user)

    find_attribute '"ns.xattrs.path": "/"' '"xattrs.nb_children": 1'
    find_attribute '"ns.xattrs.path": "/dir1"' '"xattrs.nb_children": 3'
    find_attribute '"ns.xattrs.path": "/dir1/dir2"' '"xattrs.nb_children": 2'
    find_attribute '"ns.xattrs.path": "/dir1/dir2/dir3"'\
                   '"xattrs.nb_children": 0'
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
    local nb_directories=$(count_documents '"xattrs.nb_children": {$gt: 0}')

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
                       '"xattrs.nb_children": '"$expected_children"''
    done
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_sync_2_files test_sync_size test_sync_3_files
                  test_sync_xattrs test_sync_subdir test_sync_large_tree
                  test_sync_one_one_file test_sync_one test_sync_one_two_files
                  test_sync_symbolic_link test_sync_socket test_sync_fifo
                  test_sync_branch test_continue_sync_on_error
                  test_stop_sync_on_error test_config test_sync_number_children)

if [[ $WITH_MPI == true ]]; then
    tests+=(test_sync_number_children_mpi test_sync_large_path)
fi

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
add_test_user $test_user
trap -- "rm -rf '$tmpdir'; delete_test_user $test_user" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
