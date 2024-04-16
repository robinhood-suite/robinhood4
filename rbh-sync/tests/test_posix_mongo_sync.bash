#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2021 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

rbh_sync_posix()
{
    if [[ "$WITH_MPI" == "true" ]]; then
        rbh_sync "rbh:posix-mpi:$1" "$2"
    else
        rbh_sync "rbh:posix:$1" "$2"
    fi
}

rbh_sync_posix_one()
{
    if [[ "$WITH_MPI" == "true" ]]; then
        rbh_sync -o "rbh:posix-mpi:$1" "$2"
    else
        rbh_sync -o "rbh:posix:$1" "$2"
    fi
}

################################################################################
#                                    TESTS                                     #
################################################################################

test_sync_2_files()
{
    truncate -s 1k "fileA"

    rbh_sync_posix "." "rbh:mongo:$testdb"
    find_attribute '"ns.xattrs.path":"/"'
    find_attribute '"ns.xattrs.path":"/fileA"'
}

test_sync_size()
{
    truncate -s 1025 "fileA"
    local length=$(stat -c %s "fileA")

    rbh_sync_posix "." "rbh:mongo:$testdb"
    find_attribute '"ns.xattrs.path":"/fileA"' '"statx.size" : '$length
}

test_sync_3_files()
{
    truncate -s 1k "fileA"
    truncate -s 1k "fileB"

    rbh_sync_posix "." "rbh:mongo:$testdb"
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

    rbh_sync_posix "." "rbh:mongo:$testdb"
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

    rbh_sync_posix "." "rbh:mongo:$testdb"
    find_attribute '"ns.xattrs.path":"/"'
    find_attribute '"ns.xattrs.path":"/fileA"'
    find_attribute '"ns.xattrs.path":"/fileB"'
    find_attribute '"ns.xattrs.path":"/dir"'
    find_attribute '"ns.xattrs.path":"/dir/file"'
}

test_sync_large_tree()
{
    mkdir -p {1..9}/{1..9}

    rbh_sync_posix "." "rbh:mongo:$testdb"
    for i in $(find *); do
        find_attribute '"ns.xattrs.path":"/'$i'"'
    done
}

test_sync_one_one_file()
{
    truncate -s 1k "fileA"
    local length=$(stat -c %s "fileA")

    rbh_sync_posix_one "fileA" "rbh:mongo:$testdb"
    find_attribute '"statx.size" : '$length
}

test_sync_one_two_files()
{
    truncate -s 1k "fileA"
    truncate -s 1k "fileB"
    setfattr -n user.a -v b "fileB"
    local length=$(stat -c %s "fileB")

    rbh_sync_posix_one "fileA" "rbh:mongo:$testdb"
    rbh_sync_posix_one "fileB" "rbh:mongo:$testdb"
    find_attribute '"statx.size" : '$length \
                   '"xattrs.user.a" : { $exists : true }'

    local output_lines=$(mongo $testdb --eval "db.entries.count()")
    if [[ $output_lines -ne 2 ]]; then
        error "Invalid number of files were synced, expected '2' lines, " \
              "found '$output_lines'."
    fi
}

check_mode_and_type()
{
    local entry="$1"

    local raw_mode="$(statx -c="+%f" "$entry")"
    raw_mode=${raw_mode:2}
    raw_mode=$(echo "ibase=16; ${raw_mode^^}" | bc)
    local type=$((raw_mode & 00170000))
    local mode=$((raw_mode & ~00170000))

    find_attribute '"statx.type":'$type
    find_attribute '"statx.mode":'$mode
}

test_sync_symbolic_link()
{
    local entry="symbolic_link"

    touch ${entry}_target
    ln -s ${entry}_target $entry

    rbh_sync -o "rbh:posix:$entry" "rbh:mongo:$testdb"
    check_mode_and_type $entry
}

test_sync_socket()
{
    local entry="socket_file"

    python3 -c "import socket as s; \
                sock = s.socket(s.AF_UNIX); \
                sock.bind('$entry')"

    rbh_sync_posix_one "$entry" "rbh:mongo:$testdb"
    check_mode_and_type $entry
}

test_sync_fifo()
{
    local entry="fifo_file"

    mkfifo $entry

    rbh_sync_posix_one "$entry" "rbh:mongo:$testdb"
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

    rbh_sync_posix "$first_dir#$second_dir" "rbh:mongo:$testdb"

    find_attribute '"ns.name":"'$second_dir'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir'"'
    find_attribute '"ns.name":"'$third_dir'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir/$third_dir'"'
    find_attribute '"ns.name":"'$entry'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir/$third_dir/$entry'"'

    mongo $testdb --eval "db.dropDatabase()"

    local abs_path="$(realpath $first_dir)"

    rbh_sync_posix "$abs_path#$second_dir" "rbh:mongo:$testdb"

    find_attribute '"ns.name":"'$second_dir'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir'"'
    find_attribute '"ns.name":"'$third_dir'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir/$third_dir'"'
    find_attribute '"ns.name":"'$entry'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir/$third_dir/$entry'"'

    mongo $testdb --eval "db.dropDatabase()"

    rbh_sync_posix "$first_dir#$second_dir/$third_dir" "rbh:mongo:$testdb"

    find_attribute '"ns.name":"'$third_dir'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir/$third_dir'"'
    find_attribute '"ns.name":"'$entry'"'
    find_attribute '"ns.xattrs.path":"'/$second_dir/$third_dir/$entry'"'
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

    # Here, we create a test user, and use it to run a rbh-sync on the files
    # created above. Since that user doesn't have the read or write access to
    # the second file and the directory, it cannot synchronize both, so errors
    # should be outputted but the command shouldn't fail.
    useradd -N -M test

    if [[ "$WITH_MPI" == "true" ]]; then
        # We need to give execute permissions to the user for mpirun to run
        chmod o+x ..
        output="$((sudo -E -H -u test bash -c \
                "source /etc/profile.d/modules.sh; \
                 module load mpi/openmpi-x86_64; mpirun -np 4 \
                 rbh-sync rbh:posix-mpi:. rbh:mongo:$testdb") 2>&1)"
    else
        output="$((sudo -E -H -u test bash -c "rbh-sync rbh:posix:. \
                   rbh:mongo:$testdb") 2>&1)"
    fi

    userdel -f -r test || true

    echo "$output" | grep "open '/$second_file'" ||
        error "Failed to find error on open of '$second_file'"
    echo "$output" | grep "open '/$dir'" ||
        error "Failed to find error on open of '$second_file'"

    if [[ "$WITH_MPI" == "true" ]]; then
        # We check if there is an error from mpifileutils while opening /dir
        echo "$output" | \
        grep "ERROR: Failed to open directory with opendir: './$dir'" || \
        error "Failed to find error on open of '$second_file'"
    else
        echo "$output" | grep "FTS" | grep "read entry './$dir'" ||
            error "Failed to find error on open of '$second_file'"
    fi

    local db_count=$(mongo $testdb --eval "db.entries.count()")
    if [[ $db_count -ne 2 ]]; then
        error "Invalid number of files were synced, expected '2' entries, " \
              "found '$db_count'."
    fi

    find_attribute '"ns.xattrs.path":"/"'
    find_attribute '"ns.name":"'$first_file'"'
}

test_stop_sync_on_error(){
    local first_file="test1"
    local second_file="test2"
    local third_file="test3"
    local dir="dir"

    touch $first_file
    touch $second_file
    mkdir $dir
    touch $dir/$third_file

    chmod o-rw $second_file
    chmod o-rw $dir

    # Here, we create a test user, and use it to run a rbh-sync on the files
    # created above. Since that user doesn't have the read or write access to
    # the second file and the directory, it cannot synchronize both, the
    # command should fail when synchronizing the second file and the directory

    useradd -N -M test

    if [[ "$WITH_MPI" == "true" ]]; then
        # We need to give execute permissions to the user for mpirun to run
        chmod o+x ..
        local output=$((sudo -E -H -u test bash -c \
                        "source /etc/profile.d/modules.sh; \
                        module load mpi/openmpi-x86_64; mpirun -np 4 \
                        rbh-sync --no-skip rbh:posix-mpi:. \
                        rbh:mongo:$testdb") 2>&1)
    else
        local output=$((sudo -E -H -u test bash -c "rbh-sync --no-skip rbh:posix:. \
                   rbh:mongo:$testdb") 2>&1)
    fi

    userdel -f -r test || true

    local db_count=$(mongo $testdb --eval "db.entries.count()")
    if [[ $db_count -lt 1 ]]; then
        error "Invalid number of files were synced, expected at least '1'" \
            "entries found (root) '$db_count'."
    fi

    find_attribute '"ns.xattrs.path":"/"'

    local first_file_att="$(find_attribute '"ns.name":"'$first_file'"')"
    local second_file_att="$(find_attribute '"ns.name":"'$second_file'"')"
    local third_file_att="$(find_attribute '"ns.name":"'$third_file'"')"

    # Nothing guarantees the order of synchronization, the command can start
    # with the second file and fail, or any other file or directory

    # First check if any files have been synchronized, then check that the
    # first file has been synchronized and the others have not

    (echo $second_file_att | grep "No entry found" && \
     echo $third_file_att | grep "No entry found") || \
    error "Synchronized files that should not."

}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_sync_2_files test_sync_size test_sync_3_files
                  test_sync_xattrs test_sync_subdir test_sync_large_tree
                  test_sync_one_one_file test_sync_one_two_files
                  test_sync_symbolic_link test_sync_socket test_sync_fifo
                  test_sync_branch test_continue_sync_on_error
                  test_stop_sync_on_error )

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
