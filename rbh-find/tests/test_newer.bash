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
    touch fileA
    sleep 1
    touch -d "5 minutes ago" fileB
    touch -d "10 minutes ago" fileC

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -newer /fileC | sort |
        difflines "/" "/fileA" "/fileB"
    rbh_find "rbh:mongo:$testdb" -newer /fileB | sort |
        difflines "/" "/fileA"

    rbh_find "rbh:mongo:$testdb" -newer /fileA | sort |
        difflines "/"
    rbh_find "rbh:mongo:$testdb" -not -newer /fileA | sort |
        difflines "/fileA" "/fileB" "/fileC"

    rbh_find "rbh:mongo:$testdb" -newer / | sort | difflines

    rbh_find "rbh:mongo:$testdb" -newer /fileB -a -newer /fileC | sort |
        difflines "/" "/fileA"
    rbh_find "rbh:mongo:$testdb" -newer /fileB -o -newer /fileC | sort |
        difflines "/" "/fileA" "/fileB"

    # /file doesn't exist, it should return nothing
    rbh_find "rbh:mongo:$testdb" "$1" /file | sort | difflines
}

test_anewer()
{
    touch fileA
    sleep 1
    touch -d "5 minutes ago" fileB
    touch -d "10 minutes ago" fileC

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -anewer /fileC | sort |
        difflines "/" "/fileA" "/fileB"
    rbh_find "rbh:mongo:$testdb" -anewer /fileB | sort |
        difflines "/" "/fileA"

    rbh_find "rbh:mongo:$testdb" -anewer /fileA | sort | difflines
    rbh_find "rbh:mongo:$testdb" -not -anewer /fileA | sort |
        difflines "/" "/fileA" "/fileB" "/fileC"

    rbh_find "rbh:mongo:$testdb" -anewer / | sort | difflines

    rbh_find "rbh:mongo:$testdb" -anewer /fileB -a -anewer /fileC | sort |
        difflines "/" "/fileA"
    rbh_find "rbh:mongo:$testdb" -anewer /fileB -o -anewer /fileC | sort |
        difflines "/" "/fileA" "/fileB"

    # /file doesn't exist, it should return nothing
    rbh_find "rbh:mongo:$testdb" -anewer /file | sort | difflines
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_newer test_anewer)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'"  EXIT
cd "$tmpdir"

run_tests ${tests[@]}
