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

test_max_size()
{
    touch "empty"
    truncate --size 1K "1K"
    truncate --size 1025 "1K+1"
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_report --csv "rbh:mongo:$testdb" --output "max(statx.size)" |
        difflines "1025"

    truncate --size 600M "600M"
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local size_600M="$(stat -c %s 600M)"

    rbh_report --csv "rbh:mongo:$testdb" --output "max(statx.size)" |
        difflines "$size_600M"
}

test_max_mtime()
{
    touch first
    touch -d "2 hours ago" second
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local first_mtime="$(stat -c %Y first)"

    rbh_report --csv "rbh:mongo:$testdb" --output "max(statx.mtime.sec)" |
        difflines "$first_mtime"

    touch -d "+2 hours" third
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local third_mtime="$(stat -c %Y third)"

    rbh_report --csv "rbh:mongo:$testdb" --output "max(statx.mtime.sec)" |
        difflines "$third_mtime"
}

test_max_ino()
{
    touch first
    touch second
    touch third
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local max_ino="$(find -printf "%i\n" | sort -nr | head -n 1)"

    rbh_report --csv "rbh:mongo:$testdb" --output "max(statx.ino)" |
        difflines "$max_ino"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_max_size test_max_mtime test_max_ino)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
