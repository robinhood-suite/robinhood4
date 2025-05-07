#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_filter_type()
{
    truncate --size 1K "1K"
    truncate --size 1025 "1K+1"

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local dir_size="$(stat -c %s .)"
    local file_size="$((1024 + 1025))"
    local size="$(($file_size + $dir_size))"

    rbh_report "rbh:$db:$testdb" --output "sum(statx.size)" --csv -type f |
        difflines "$file_size"
    rbh_report "rbh:$db:$testdb" --output "sum(statx.size)" --csv -type d |
        difflines "$dir_size"
    rbh_report "rbh:$db:$testdb" --output "sum(statx.size)" --csv \
        -type f -a -type d | difflines
    rbh_report "rbh:$db:$testdb" --output "sum(statx.size)" --csv \
        -type f -o -type d | difflines "$size"
}

test_filter_size()
{
    touch "empty"
    truncate --size 1K "1K"
    truncate --size 1025 "1K+1"

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local dir_size="$(stat -c %s .)"
    local file_size="$((1024 + 1025))"
    local size="$(($file_size + $dir_size))"

    rbh_report "rbh:$db:$testdb" --output "sum(statx.size)" --csv -size +0 |
        difflines "$size"
    rbh_report "rbh:$db:$testdb" --output "sum(statx.size)" --csv -size 1k |
        difflines "$((1024 + $dir_size))"
    rbh_report "rbh:$db:$testdb" --output "sum(statx.size)" --csv \
        -size +1k | difflines "1025"
    rbh_report "rbh:$db:$testdb" --output "sum(statx.size)" --csv \
        -type d -a -size 1k | difflines "$dir_size"
}

test_filter_to_complete()
{
    truncate --size 1K "1K"
    touch -d "1 minutes ago" "1K"
    truncate --size 1025 "1K+1"
    touch -d "5 minutes ago" "1K+1"
    touch -d "10 minutes ago" empty

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local dir_size="$(stat -c %s .)"

    rbh_report "rbh:$db:$testdb" --output "sum(statx.size)" --csv \
        -newer /empty | difflines "$(($dir_size + 1024 + 1025))"
    rbh_report "rbh:$db:$testdb" --output "sum(statx.size)" --csv \
        -newer "/1K+1" | difflines "$(($dir_size + 1024))"
    rbh_report "rbh:$db:$testdb" --output "sum(statx.size)" --csv -newer "/1K" |
        difflines "$dir_size"
    rbh_report "rbh:$db:$testdb" --output "sum(statx.size)" --csv -newer "/" |
        difflines
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_filter_type test_filter_size test_filter_to_complete)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
