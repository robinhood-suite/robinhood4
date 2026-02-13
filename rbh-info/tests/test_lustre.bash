#!/usr/bin/env bash

# This file is part of rbh-info.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_lustre_source()
{
    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    local output=$(rbh_info "rbh:$db:$testdb" -b)
    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != 3)); then
        error "Three backends should have been registered"
    fi

    echo "$output" | grep "posix" ||
        error "The POSIX plugin should have been registered"

    echo "$output" | grep "lustre" ||
        error "The Lustre extension should have been registered"

    echo "$output" | grep "retention" ||
        error "The retention extension should have been registered"
}

test_command_backend()
{
    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    local command_backend=$(rbh_info "rbh:$db:$testdb" -B)

    if [ "$command_backend" != "lustre" ]; then
        error "Command backends don't match, found '$command_backend', expected 'lustre'\n"
    fi

    command_backend=$(rbh_info "rbh:$db:$testdb" --command-backend)

    if [ "$command_backend" != "lustre" ]; then
        error "Command backends don't match, found '$command_backend', expected 'lustre'\n"
    fi
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_lustre_source test_command_backend)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
