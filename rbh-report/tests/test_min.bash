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

test_min_size()
{
    truncate --size 1K "1K"
    truncate --size 1025 "1K+1"
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local root_size="$(stat -c %s .)"

    rbh_report --csv "rbh:mongo:$testdb" --output "min(statx.size)" |
        difflines "$root_size"

    touch blob
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_report --csv "rbh:mongo:$testdb" --output "min(statx.size)" |
        difflines "0"
}

test_min_mtime()
{
    touch first
    touch -d "+2 hours" second
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local first_mtime="$(stat -c %Y first)"

    rbh_report --csv "rbh:mongo:$testdb" --output "min(statx.mtime.sec)" |
        difflines "$first_mtime"

    touch -d "-2 hours" third
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local third_mtime="$(stat -c %Y third)"

    rbh_report --csv "rbh:mongo:$testdb" --output "min(statx.mtime.sec)" |
        difflines "$third_mtime"
}

test_min_ino()
{
    touch first
    touch second
    touch third
    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    local min_ino="$(find -printf "%i\n" | sort -n | head -n 1)"

    rbh_report --csv "rbh:mongo:$testdb" --output "min(statx.ino)" |
        difflines "$min_ino"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_min_size test_min_mtime test_min_ino)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
