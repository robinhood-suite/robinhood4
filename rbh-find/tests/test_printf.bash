#!/usr/bin/env bash

# This file is part of the RobinHood Library
# Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

if ! command -v rbh-sync &> /dev/null; then
    echo "This test requires rbh-sync to be installed" >&2
    exit 1
fi

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_atime()
{
    touch -a -d "$(date -d '1 hour')" file
    touch -m -d "$(date -d '2 hours')" file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local date=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%a\n")
    local d=$(date -d "$date")
    local atime=$(date -d "@$(stat -c %X file)")

    if [[ "$d" != "$atime" ]]; then
        error "printf atime: '$d' != actual '$atime'"
    fi
}

test_ctime()
{
    touch -a -d "$(date -d '1 hour')" file
    touch -m -d "$(date -d '2 hours')" file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local date=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%c\n")
    local d=$(date -d "$date")
    local ctime=$(date -d "@$(stat -c %Z file)")

    if [[ "$d" != "$ctime" ]]; then
        error "printf ctime: '$d' != actual '$ctime'"
    fi
}

test_mtime()
{
    touch -a -d "$(date -d '1 hour')" file
    touch -m -d "$(date -d '2 hours')" file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local date=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%t\n")
    local d=$(date -d "$date")
    local mtime=$(date -d "@$(stat -c %Y file)")

    if [[ "$d" != "$mtime" ]]; then
        error "printf mtime: '$d' != actual '$mtime'"
    fi
}

test_filename()
{
    touch file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh-find "rbh:mongo:$testdb" -name file -printf "%f\n" | sort |
        difflines "file"
}

test_inode()
{
    touch file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local i=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%i\n")
    local inode=$(stat -c %i file)

    if [[ $i != $inode ]]; then
        error "printf inode: $i != actual $inode"
    fi
}

test_uid()
{
    touch file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local u=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%U\n")
    local uid=$(stat -c %u file)

    if [[ $u != $uid ]]; then
        error "printf UID: $u != actual $uid"
    fi
}

test_gid()
{
    touch file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local g=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%G\n")
    local gid=$(stat -c %g file)

    if [[ $g != $gid ]]; then
        error "printf GID: $g != actual $gid"
    fi
}

test_username()
{
    touch file
    touch file_without_user
    useradd test
    chown test: file_without_user

    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"
    userdel test

    local u=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%u\n")
    local name=$(stat -c %U file)

    if [[ $u != $name ]]; then
        error "printf user name: $u != actual $name"
    fi

    local u=$(rbh-find "rbh:mongo:$testdb" -name file_without_user \
                  -printf "%u\n")
    local uid=$(stat -c %u file_without_user)

    if [[ $u != $uid ]]; then
        error "printf UID: $u != actual $uid"
    fi
}

test_groupname()
{
    touch file
    touch file_without_user
    useradd test
    chown test: file_without_user

    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"
    userdel test

    local g=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%g\n")
    local name=$(stat -c %G file)

    if [[ $g != $name ]]; then
        error "wrong group name: $g != $name"
    fi

    local g=$(rbh-find "rbh:mongo:$testdb" -name file_without_user \
                  -printf "%g\n")
    local gid=$(stat -c %g file_without_user)

    if [[ $g != $gid ]]; then
        error "printf GID: $g != actual $gid"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_atime test_ctime test_mtime test_filename test_inode
                  test_uid test_gid test_username test_groupname)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
