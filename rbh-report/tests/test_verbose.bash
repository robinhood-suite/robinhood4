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

test_verbose()
{
    truncate --size 1K "1K"
    truncate --size 1025 "1K+1"

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local output="$(rbh_report --verbose "rbh:$db:$testdb" \
                               --csv --output "sum(statx.size)")"

    local dir_size="$(stat -c %s .)"
    local size="$((1024 + 1025 + dir_size))"

    echo "$output" | grep "$size"
    echo "$output" | grep "\$group" | grep "_id" | grep "0" |
                     grep "sum_statx_size" | grep "\$statx.size"

    local output="$(rbh_report --verbose "rbh:$db:$testdb" \
                               --csv --group-by "statx.type" \
                               --output "sum(statx.size)")"

    echo "$output" | grep "directory" | grep "$dir_size"
    echo "$output" | grep "file" | grep "2049"

    echo "$output" | grep "\$group" | grep "_id" | grep "statx_type" |
                     grep "\$statx.type"
}

test_verbose_dry_run()
{
    truncate --size 1K "1K"
    truncate --size 1025 "1K+1"

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    local output="$(rbh_report --verbose --dry-run "rbh:$db:$testdb" \
                               --csv --output "sum(statx.size)")"

    local dir_size="$(stat -c %s .)"
    local size="$((1024 + 1025 + dir_size))"

    echo "$output" | grep "$size" &&
        error "'$size' shouldn't have been returned"
    echo "$output" | grep "\$group" | grep "_id" | grep "0" |
                     grep "sum_statx_size" | grep "\$statx.size"

    local output="$(rbh_report --verbose --dry-run "rbh:$db:$testdb" \
                               --csv --group-by "statx.type" \
                               --output "sum(statx.size)")"

    echo "$output" | grep "directory" | grep "$dir_size" &&
        error "'directory' report shouldn't have been returned"
    echo "$output" | grep "file" | grep "2049" &&
        error "'directory' report shouldn't have been returned"

    echo "$output" | grep "\$group" | grep "_id" | grep "statx_type" |
                     grep "\$statx.type"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_verbose test_verbose_dry_run)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'"  EXIT
cd "$tmpdir"

run_tests ${tests[@]}
