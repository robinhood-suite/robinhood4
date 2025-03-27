#!/usr/bin/env bash

# This file is part of the RobinHood Library
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_atime()
{
    touch -a -d "$(date -d '1 hour')" file
    touch -m -d "$(date -d '2 hours')" file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local date=$(rbh_find "rbh:$db:$testdb" -name file -printf "%a\n")
    local d=$(date -d "$date")
    local atime=$(date -d "@$(stat -c %X file)")

    if [[ "$d" != "$atime" ]]; then
        error "printf atime: '$d' != actual '$atime'"
    fi

    date=$(rbh_find "rbh:$db:$testdb" -name file -printf "%A\n")
    atime=$(stat -c %X file)

    if [[ "$date" != "$atime" ]]; then
        error "printf atime: '$date' != actual '$atime'"
    fi
}

test_ctime()
{
    touch -a -d "$(date -d '1 hour')" file
    touch -m -d "$(date -d '2 hours')" file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local date=$(rbh_find "rbh:$db:$testdb" -name file -printf "%c\n")
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

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local date=$(rbh_find "rbh:$db:$testdb" -name file -printf "%t\n")
    local d=$(date -d "$date")
    local mtime=$(date -d "@$(stat -c %Y file)")

    if [[ "$d" != "$mtime" ]]; then
        error "printf mtime: '$d' != actual '$mtime'"
    fi

    date=$(rbh_find "rbh:$db:$testdb" -name file -printf "%T\n")
    mtime=$(stat -c %Y file)

    if [[ "$date" != "$mtime" ]]; then
        error "printf mtime: '$date' != actual '$mtime'"
    fi
}

test_filename()
{
    touch file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -name file -printf "%f\n" | sort |
        difflines "file"
}

test_inode()
{
    touch file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local i=$(rbh_find "rbh:$db:$testdb" -name file -printf "%i\n")
    local inode=$(stat -c %i file)

    if [[ $i != $inode ]]; then
        error "printf inode: $i != actual $inode"
    fi
}

test_uid()
{
    touch file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local u=$(rbh_find "rbh:$db:$testdb" -name file -printf "%U\n")
    local uid=$(stat -c %u file)

    if [[ $u != $uid ]]; then
        error "printf UID: $u != actual $uid"
    fi
}

test_gid()
{
    touch file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local g=$(rbh_find "rbh:$db:$testdb" -name file -printf "%G\n")
    local gid=$(stat -c %g file)

    if [[ $g != $gid ]]; then
        error "printf GID: $g != actual $gid"
    fi
}

test_username()
{
    touch file
    touch file_without_user

    add_test_user $test_user
    chown $test_user: file_without_user
    delete_test_user $test_user

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local u=$(rbh_find "rbh:$db:$testdb" -name file -printf "%u\n")
    local name=$(stat -c %U file)

    if [[ $u != $name ]]; then
        error "printf user name: $u != actual $name"
    fi

    local u=$(rbh_find "rbh:$db:$testdb" -name file_without_user \
                  -printf "%u\n")
    local uid=$(stat -c %u file_without_user)

    if [[ $u != $uid ]]; then
        error "printf UID: '$u' != actual '$uid'"
    fi
}

test_groupname()
{
    touch file
    touch file_without_user

    useradd -K MAIL_DIR=/dev/null -lMU $test_user
    chown $test_user: file_without_user
    userdel $test_user

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local g=$(rbh_find "rbh:$db:$testdb" -name file -printf "%g\n")
    local name=$(stat -c %G file)

    if [[ $g != $name ]]; then
        error "wrong group name: $g != $name"
    fi

    local g=$(rbh_find "rbh:$db:$testdb" -name file_without_user \
                  -printf "%g\n")
    local gid=$(stat -c %g file_without_user)

    if [[ $g != $gid ]]; then
        error "printf GID: '$g' != actual '$gid'"
    fi
}

test_backend_name()
{
    mongo "other" --eval "db.dropDatabase()" >/dev/null
    mongo "${testdb}2" --eval "db.dropDatabase()" >/dev/null

    rbh_sync "rbh:posix:." "rbh:$db:other"
    touch file
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    rbh_sync "rbh:posix:." "rbh:$db:${testdb}2"

    rbh_find "rbh:$db:$testdb" "rbh:$db:other" "rbh:$db:${testdb}2" \
        -name file -printf "%H\n" | sort |
            difflines "rbh:$db:$testdb" "rbh:$db:${testdb}2"

    mongo "other" --eval "db.dropDatabase()" >/dev/null
    mongo "${testdb}2" --eval "db.dropDatabase()" >/dev/null
}

test_size()
{
    truncate -s 512 file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local s=$(rbh_find "rbh:$db:$testdb" -name file -printf "%s\n")
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
    mknod block b 0 0 || exit 77
    mknod char c 0 0

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%p %y\n" | sort |
        difflines "/ d" "/block b" "/char c" "/fifo p" "/file f" "/hlink f" \
            "/slink l"
}

test_symlink()
{
    touch file
    ln -s file slink

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    # %l for files other than symlink return an empty string
    rbh_find "rbh:$db:$testdb" -name file -printf "%l\n" |
        difflines ""

    rbh_find "rbh:$db:$testdb" -name slink -printf "%l\n" |
        difflines "file"
}

test_percent_sign()
{
    touch file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%%\n" |
        difflines "%" "%"
}

test_blocks()
{
    dd oflag=direct if=/dev/urandom of=blocks count=4
    touch file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local b=$(rbh_find "rbh:$db:$testdb" -name file -printf "%b\n")
    local blocks=$(stat -c %b file)

    if [[ $b != $blocks ]]; then
        error "wrong blocks: printf output '$b' != stat output '$blocks'"
    fi

    b=$(rbh_find "rbh:$db:$testdb" -name blocks -printf "%b\n")
    blocks=$(stat -c %b blocks)

    if [[ $b != $blocks ]]; then
        error "wrong blocks: printf output '$b' != stat output '$blocks'"
    fi
}

test_depth()
{
    mkdir -p a/b/c/d/e

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%d\n" | sort |
        difflines "$(find . -printf "%d\n")"
}

test_device()
{
    touch file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local D=$(rbh_find "rbh:$db:$testdb" -name file -printf "%D\n")
    local device=$(stat -c %d file)

    if [[ $D != $device ]]; then
        error "wrong device: printf output '$D' != stat output '$device'"
    fi
}

test_dirname()
{
    mkdir -p a/b/c/d/e

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%h\n" | sort |
        difflines "/" "/" "/a" "/a/b" "/a/b/c" "/a/b/c/d"
}

test_symbolic_permission()
{
    touch file
    chmod 461 file
    touch special1
    chmod 7624 special1
    ln -s special1 link
    touch special2
    chmod 7777 special2
    mkdir dir
    mknod block b 1 2 || exit 77

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local find_output="$(find . -printf "%M\n" | sort)"

    rbh_find "rbh:$db:$testdb" -printf "%M\n" | sort |
        difflines "$find_output"
}

test_octal_permission()
{
    touch file
    chmod 461 file
    mkdir dir

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local find_output="$(find . -printf "%m\n" | sort)"

    rbh_find "rbh:$db:$testdb" -printf "%m\n" | sort |
        difflines "$find_output"
}

test_hardlink()
{
    touch file
    ln file hlink1
    touch blob
    ln blob hlink2
    ln blob hlink3
    touch dummy

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local N1=$(rbh_find "rbh:$db:$testdb" -name file -printf "%n\n")
    local hardlink1=$(stat -c %h file)

    if [[ $N1 != $hardlink1 ]]; then
        error "wrong number of hard links: printf output '$N1' !=" \
              "stat output '$hardlink1'"
    fi

    local N2=$(rbh_find "rbh:$db:$testdb" -name blob -printf "%n\n")
    local hardlink2=$(stat -c %h blob)

    if [[ $N2 != $hardlink2 ]]; then
        error "wrong number of hard links: printf output '$N2' !=" \
              "stat output '$hardlink2'"
    fi

    local N3=$(rbh_find "rbh:$db:$testdb" -name dummy -printf "%n\n")
    local hardlink3=$(stat -c %h dummy)

    if [[ $N3 != $hardlink3 ]]; then
        error "wrong number of hard links: printf output '$N3' !=" \
              "stat output '$hardlink3'"
    fi
}

test_path_without_start()
{
    touch file
    mkdir -p tmp/test
    touch tmp/test/blob

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -printf "%P\n" | sort |
        difflines "" "file" "tmp" "tmp/test" "tmp/test/blob"

    rbh_find "rbh:$db:$testdb#tmp" -printf "%P\n" | sort |
        difflines "" "test" "test/blob"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_atime test_ctime test_mtime test_filename test_inode
                  test_uid test_gid test_username test_groupname
                  test_backend_name test_size test_type test_symlink
                  test_percent_sign test_blocks test_depth test_device
                  test_dirname test_symbolic_permission test_octal_permission
                  test_hardlink test_path_without_start)

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
trap -- "rm -rf '$tmpdir'; delete_test_user $test_user || true" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
