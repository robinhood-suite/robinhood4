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

test_newer()
{
    touch -d "1 minutes ago" fileA
    touch -d "5 minutes ago" fileB
    touch -d "10 minutes ago" fileC

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -newer /fileC | sort |
        difflines "/" "/fileA" "/fileB"
    rbh_find "rbh:$db:$testdb" -newer /fileB | sort |
        difflines "/" "/fileA"

    rbh_find "rbh:$db:$testdb" -newer /fileA | sort |
        difflines "/"
    rbh_find "rbh:$db:$testdb" -not -newer /fileA | sort |
        difflines "/fileA" "/fileB" "/fileC"

    rbh_find "rbh:$db:$testdb" -newer / | sort | difflines

    rbh_find "rbh:$db:$testdb" -newer /fileB -a -newer /fileC | sort |
        difflines "/" "/fileA"
    rbh_find "rbh:$db:$testdb" -newer /fileB -o -newer /fileC | sort |
        difflines "/" "/fileA" "/fileB"

    # /file doesn't exist, it should return nothing
    rbh_find "rbh:$db:$testdb" -newer /file | sort | difflines
}

test_anewer()
{
    touch fileA
    touch -d "5 minutes ago" fileB
    touch -d "10 minutes ago" fileC

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -anewer /fileC | sort |
        difflines "/" "/fileA" "/fileB"
    rbh_find "rbh:$db:$testdb" -anewer /fileB | sort |
        difflines "/" "/fileA"

    rbh_find "rbh:$db:$testdb" -anewer /fileA | sort | difflines
    rbh_find "rbh:$db:$testdb" -not -anewer /fileA | sort |
        difflines "/" "/fileA" "/fileB" "/fileC"

    rbh_find "rbh:$db:$testdb" -anewer / | sort | difflines

    rbh_find "rbh:$db:$testdb" -anewer /fileB -a -anewer /fileC | sort |
        difflines "/" "/fileA"
    rbh_find "rbh:$db:$testdb" -anewer /fileB -o -anewer /fileC | sort |
        difflines "/" "/fileA" "/fileB"

    # /file doesn't exist, it should return nothing
    rbh_find "rbh:$db:$testdb" -anewer /file | sort | difflines
}

test_cnewer()
{
    touch fileA
    sleep 1
    touch fileB

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -cnewer /fileB | sort | difflines
    rbh_find "rbh:$db:$testdb" -not -cnewer /fileB | sort |
        difflines "/" "/fileA" "/fileB"

    rbh_find "rbh:$db:$testdb" -cnewer /fileA | sort |
        difflines "/" "/fileB"

    rbh_find "rbh:$db:$testdb" -cnewer /fileA -a -cnewer /fileB | sort |
        difflines
    rbh_find "rbh:$db:$testdb" -cnewer /fileA -o -cnewer /fileB | sort |
        difflines "/" "/fileB"

    rbh_find "rbh:$db:$testdb" -cnewer /file | sort | difflines
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_newer test_anewer test_cnewer)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'"  EXIT
cd "$tmpdir"

run_tests ${tests[@]}
