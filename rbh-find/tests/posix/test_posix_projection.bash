#!/usr/bin/env bash

# This file is part of the RobinHood Library
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_print()
{
    touch file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local output="$(rbh_find --verbose "rbh:$db:$testdb")"

    echo "$output" | grep "\$project" | grep '"ns" : { "xattrs" : true }'
    echo "$output" | grep "/file"
}

test_print0()
{
    touch file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local output="$(rbh_find --verbose "rbh:$db:$testdb" -print0 "")"

    echo "$output" | grep "\$project" | grep '"ns" : { "xattrs" : true }'
    echo "$output" | grep "/file"
}

test_ls()
{
    touch file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local output="$(rbh_find --verbose "rbh:$db:$testdb" -ls "")"

    echo "$output" | grep "\$project" |
        grep '"ns" : { "xattrs" : true }, "statx" : { "type" : true,
             "mode" : true, "nlink": true, "uid": true, "gid" : true,
             "mtime" : { "sec" : true }, "ino" : true, "size" : true,
             "blocks" : true }, "symlink" : true }'
    echo "$output" | grep "/file"
}

check_printf_project()
{
    local printf=$1
    local project_string=$2

    local output="$(rbh_find --verbose "rbh:$db:$testdb" -printf "$printf")"

    echo "$output" | grep "\$project" | grep "$project_string"
}

test_printf()
{
    touch file

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    check_printf_project "%a\n" '"statx" : { "atime" : { "sec" : true } }'
    check_printf_project "%A\n" '"statx" : { "atime" : { "sec" : true } }'
    check_printf_project "%b\n" '"statx" : { "blocks" : true }'
    check_printf_project "%c\n" '"statx" : { "ctime" : { "sec" : true } }'
    check_printf_project "%d\n" '"ns" : { "xattrs" : true }'
    check_printf_project "%h\n" '"ns" : { "xattrs" : true }'
    check_printf_project "%p\n" '"ns" : { "xattrs" : true }'
    check_printf_project "%P\n" '"ns" : { "xattrs" : true }'
    check_printf_project "%D\n" \
        '"statx" : { "dev" : { "major" : true, "minor" : true }'
    check_printf_project "%f\n" '"ns" : { "name" : true }'
    check_printf_project "%g\n" '"statx" : { "gid" : true }'
    check_printf_project "%G\n" '"statx" : { "gid" : true }'
    check_printf_project "%i\n" '"statx" : { "ino" : true }'
    check_printf_project "%I\n" '"_id" : true'
    check_printf_project "%l\n" '"statx" : { "type" : true, "mode" : true }, "symlink" : true'
    check_printf_project "%m\n" '"statx" : { "mode" : true }'
    check_printf_project "%M\n" '"statx" : { "mode" : true }'
    check_printf_project "%y\n" '"statx" : { "mode" : true }'
    check_printf_project "%n\n" '"statx" : { "nlink" : true }'
    check_printf_project "%s\n" '"statx" : { "size" : true }'
    check_printf_project "%t\n" '"statx" : { "mtime" : { "sec" : true } }'
    check_printf_project "%T\n" '"statx" : { "mtime" : { "sec" : true } }'
    check_printf_project "%u\n" '"statx" : { "uid" : true }'
    check_printf_project "%U\n" '"statx" : { "uid" : true }'

    check_printf_project "%s %i\n" '"statx" : { "ino" : true, "size" : true }'
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_print test_print0 test_ls test_printf)

tmpdir=$(mktemp --directory)
test_user="$(get_test_user "$(basename "$0")")"
trap -- "rm -rf '$tmpdir'; delete_test_user $test_user || true" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
