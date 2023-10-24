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
    touch file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local date=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%a\n")
    local d=$(date -d "$date")
    local atime=$(date -d "@$(stat -c %X file)")

    if [[ "$d" != "$atime" ]]; then
        error "wrong atime: '$d' != '$atime'"
    fi
}

test_ctime()
{
    touch file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local date=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%c\n")
    local d=$(date -d "$date")
    local ctime=$(date -d "@$(stat -c %Z file)")

    if [[ "$d" != "$ctime" ]]; then
        error "wrong ctime: '$d' != '$ctime'"
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
        error "wrong inode: $i != $inode"
    fi
}

test_uid()
{
    touch file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local u=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%U\n")
    local uid=$(stat -c %u file)

    if [[ $u != $uid ]]; then
        error "wrong UID: $u != $uid"
    fi
}

test_gid()
{
    touch file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local g=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%G\n")
    local gid=$(stat -c %g file)

    if [[ $g != $gid ]]; then
        error "wrong GID: $g != $gid"
    fi
}

test_username()
{
    touch file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local u=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%u\n")
    local name=$(stat -c %U file)

    if [[ $u != $name ]]; then
        error "wrong user name: $u != $name"
    fi
}

test_groupname()
{
    touch file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local g=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%g\n")
    local name=$(stat -c %G file)

    if [[ $g != $name ]]; then
        error "wrong group name: $g != $name"
    fi
}

test_backend_name()
{
    mongo --quiet "other" --eval "db.dropDatabase()" >/dev/null
    mongo --quiet "${testdb}2" --eval "db.dropDatabase()" >/dev/null

    rbh-sync "rbh:posix:." "rbh:mongo:other"
    touch file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"
    rbh-sync "rbh:posix:." "rbh:mongo:${testdb}2"

    rbh-find "rbh:mongo:$testdb" "rbh:mongo:other" "rbh:mongo:${testdb}2" \
        -name file -printf "%H\n" | sort |
            difflines "rbh:mongo:$testdb" "rbh:mongo:${testdb}2"

    mongo --quiet "other" --eval "db.dropDatabase()" >/dev/null
    mongo --quiet "${testdb}2" --eval "db.dropDatabase()" >/dev/null
}

test_size()
{
    truncate -s 512 file
    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local s=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%s\n")
    local size=$(stat -c %s file)

    if [[ $s != $size ]]; then
        error "wrong size: $s != $size"
    fi
}

test_type()
{
    touch file
    ln file hlink
    ln -s file slink
    mkfifo fifo
    mknod block b 0 0
    mknod char c 0 0

    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh-find "rbh:mongo:$testdb" -printf "%p %y\n" | sort |
        difflines "/block b" "/char c" "/ d" "/fifo p" "/file f" "/hlink f" \
            "/slink l"
}

test_symlink()
{
    touch file
    ln -s file slink

    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    # %l for files other than symlink return an empty string
    rbh-find "rbh:mongo:$testdb" -name file -printf "%l\n" |
        difflines ""

    rbh-find "rbh:mongo:$testdb" -name slink -printf "%l\n" |
        difflines "file"
}

test_percent_sign()
{
    touch file

    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh-find "rbh:mongo:$testdb" -printf "%%\n" |
        difflines "%" "%"
}

test_blocks()
{
    touch file

    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local b=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%b\n")
    local blocks=$(stat -c %b file)

    if [[ $b != $blocks ]]; then
        error "wrong blocks: printf output '$b' != stat output '$blocks'"
    fi
}

test_depth()
{
    mkdir -p a/b/c/d/e

    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh-find "rbh:mongo:$testdb" -printf "%d\n" | sort |
        difflines "0" "1" "2" "3" "4" "5"
}

test_device()
{
    touch file

    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    local D=$(rbh-find "rbh:mongo:$testdb" -name file -printf "%D\n")
    local device=$(stat -c %d file)

    if [[ $D != $device ]]; then
        error "wrong device: printf output '$D' != stat output '$device'"
    fi
}

test_dirname()
{
    mkdir -p a/b/c/d/e

    rbh-sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh-find "rbh:mongo:$testdb" -printf "%f\n" | sort |
        difflines "." "." "a" "a/b" "a/b/c" "a/b/c/d"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_atime test_ctime test_filename test_inode test_uid
                  test_gid test_username test_groupname test_backend_name
                  test_size test_type test_symlink test_percent_sign
                  test_blocks test_depth test_device test_dirname)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
