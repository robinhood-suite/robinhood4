#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_limit()
{
    local actual

    touch file1 file2 file3 file4
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    actual=$(rbh_find "rbh:$db:$testdb" -limit 1 | wc -l)
    if (( actual != 1 )); then
        error "Expected 1 entry, got $actual"
    fi

    actual=$(rbh_find "rbh:$db:$testdb" -limit 3 | wc -l)
    if (( actual != 3 )); then
        error "Expected 3 entries, got $actual"
    fi

    actual=$(rbh_find "rbh:$db:$testdb" -limit 10 | wc -l)
    if (( actual != 5 )); then
        error "Expected 5 entries (4 files and the directory), got $actual"
    fi
}

test_unlimited()
{
    local actual

    touch file1 file2 file3 file4
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    actual=$(rbh_find "rbh:$db:$testdb" -limit 0 | wc -l)
    if (( actual != 5 )); then
        error "Expected 5 entries (4 files and the directory), got $actual"
    fi
}

test_invalid()
{
    touch file1 file2 file3 file4
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    if rbh_find "rbh:$db:$testdb" -limit >/dev/null 2>&1; then
        error "'-limit' without an argument should fail"
    fi

    if rbh_find "rbh:$db:$testdb" -limit invalid >/dev/null 2>&1; then
        error "'-limit invalid' should fail"
    fi

    if rbh_find "rbh:$db:$testdb" -limit -1 >/dev/null 2>&1; then
        error "'-limit -1' should fail"
    fi

    if rbh_find "rbh:$db:$testdb" \
            -limit 2 -limit 3 >/dev/null 2>&1; then
        error "'-limit' specified twice should fail"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_limit test_unlimited test_invalid)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'"  EXIT
cd "$tmpdir"

run_tests ${tests[@]}
