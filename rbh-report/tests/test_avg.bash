#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_avg_size()
{
    touch "empty"
    truncate --size 1K "1K"
    truncate --size 1025 "1K+1"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local dir_size="$(stat -c %s .)"
    local size="$(((1024 + 1025 + dir_size) / 4))"

    rbh_report --csv "rbh:$db:$testdb" --output "avg(statx.size)" |
        difflines "$size"

    truncate --size 600M "600M"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local size_600M="$(stat -c %s 600M)"
    dir_size="$(stat -c %s .)"
    size="$(((1024 + 1025 + size_600M + dir_size) / 5))"

    rbh_report --csv "rbh:$db:$testdb" --output "avg(statx.size)" |
        difflines "$size"
}

test_avg_mtime()
{
    touch first
    touch -d "2 hours ago" second
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local root_mtime="$(stat -c %Y .)"
    local first_mtime="$(stat -c %Y first)"
    local second_mtime="$(stat -c %Y second)"
    local mtime="$(((root_mtime + first_mtime + second_mtime) / 3))"

    rbh_report --csv "rbh:$db:$testdb" --output "avg(statx.mtime.sec)" |
        difflines "$mtime"

    touch -d "+2 hours" third
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    root_mtime="$(stat -c %Y .)"
    first_mtime="$(stat -c %Y first)"
    second_mtime="$(stat -c %Y second)"
    local third_mtime="$(stat -c %Y third)"
    mtime="$(((root_mtime + first_mtime + second_mtime + third_mtime) / 4))"

    rbh_report --csv "rbh:$db:$testdb" --output "avg(statx.mtime.sec)" |
        difflines "$mtime"
}

test_avg_ino()
{
    touch first
    touch second
    touch third
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local root_ino="$(stat -c %i .)"
    local first_ino="$(stat -c %i first)"
    local second_ino="$(stat -c %i second)"
    local third_ino="$(stat -c %i third)"
    local ino="$(((root_ino + first_ino + second_ino + third_ino) / 4))"

    rbh_report --csv "rbh:$db:$testdb" --output "avg(statx.ino)" |
        difflines "$ino"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_avg_size test_avg_mtime test_avg_ino)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
