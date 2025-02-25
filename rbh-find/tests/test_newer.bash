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

test_generic_a_newer()
{
    touch fileA
    sleep 1
    touch -d "5 minutes ago" fileB
    touch -d "10 minutes ago" fileC

    rbh_sync "rbh:posix:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" "$1" /fileC | sort |
        difflines "/" "/fileA" "/fileB"
    rbh_find "rbh:mongo:$testdb" "$1" /fileB | sort |
        difflines "/" "/fileA"

    if [[ "$1" == "-newer" ]]; then
        rbh_find "rbh:mongo:$testdb" "$1" /fileA | sort |
            difflines "/"
        rbh_find "rbh:mongo:$testdb" -not "$1" /fileA | sort |
            difflines "/fileA" "/fileB" "/fileC"

        rbh_find "rbh:mongo:$testdb" "$1" / | sort | difflines
    else
        rbh_find "rbh:mongo:$testdb" "$1" /fileA | sort | difflines
        rbh_find "rbh:mongo:$testdb" -not "$1" /fileA | sort |
            difflines "/" "/fileA" "/fileB" "/fileC"

        rbh_find "rbh:mongo:$testdb" "$1" / | sort | difflines
    fi

    rbh_find "rbh:mongo:$testdb" "$1" /fileB -a "$1" /fileC | sort |
        difflines "/" "/fileA"
    rbh_find "rbh:mongo:$testdb" "$1" /fileB -o "$1" /fileC | sort |
        difflines "/" "/fileA" "/fileB"

    # /file doesn't exist, it should return nothing
    rbh_find "rbh:mongo:$testdb" "$1" /file | sort | difflines
}

test_newer()
{
    test_generic_a_newer "-newer"
}

test_anewer()
{
    test_generic_a_newer "-anewer"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_newer test_anewer)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'"  EXIT
cd "$tmpdir"

run_tests ${tests[@]}
