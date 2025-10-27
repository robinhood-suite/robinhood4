#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copytright (C) 2025 Commissariat a l'energie atomique et aux energies
#                     alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    if rbh_find "rbh:$db:$testdb" -project-id -1; then
        error "find with a negative project id should have failed"
    fi

    if rbh_find "rbh:$db:$testdb" -project-id $(echo 2^64 | bc); then
        error "find with a project id too big should have failed"
    fi
    if rbh_find "rbh:$db:$testdb" -project-id 42blob; then
        error "find with an invalid project id should have failed"
    fi
    if rbh_find "rbh:$db:$testdb" -project-id invalid; then
        error "find with an invalid project id should have failed"
    fi
}

test_project_id()
{
    touch "project_1"
    touch "project_2"
    touch "vanilla"

    lfs project -p 1 "project_1"
    lfs project -p 2 "project_2"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -project-id 0 | sort |
        difflines "/" "/vanilla"

    rbh_find "rbh:$db:$testdb" -project-id 1 | sort |
        difflines "/project_1"

    rbh_find "rbh:$db:$testdb" -project-id 2 | sort |
        difflines "/project_2"

    rbh_find "rbh:$db:$testdb" -project-id 1 -o -project-id 2 | sort |
        difflines "/project_1" "/project_2"

    rbh_find "rbh:$db:$testdb" -not -project-id 1 | sort |
        difflines "/" "/project_2" "/vanilla"
}

################################################################################
#                                     MAIN                                     #
################################################################################


declare -a tests=(test_invalid test_project_id)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap "rm -r $tmpdir" EXIT

cd "$tmpdir"

run_tests "${tests[@]}"
