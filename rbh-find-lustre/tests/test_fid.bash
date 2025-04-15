#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

if ! lctl get_param mdt.*.hsm_control | grep "enabled"; then
    echo "At least 1 MDT needs to have HSM control enabled" >&2
    exit 1
fi

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_syntax_error()
{
    touch "dummy"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -fid fail &&
        error "command should have failed because of invalid syntax"
    rbh_find "rbh:$db:$testdb" -fid 0x3 &&
        error "command should have failed because of invalid syntax"
    rbh_find "rbh:$db:$testdb" -fid 0x3:0x4 &&
        error "command should have failed because of invalid syntax"
    rbh_find "rbh:$db:$testdb" -fid "0x3:0x4:0x5]" &&
        error "command should have failed because of invalid syntax"
    rbh_find "rbh:$db:$testdb" -fid "0x3:0x4:0x5]]" &&
        error "command should have failed because of invalid syntax"
    rbh_find "rbh:$db:$testdb" -fid "[0x3:0x4:0x5" &&
        error "command should have failed because of invalid syntax"
    rbh_find "rbh:$db:$testdb" -fid "[[0x3:0x4:0x5" &&
        error "command should have failed because of invalid syntax"
    rbh_find "rbh:$db:$testdb" -fid "[[0x3:0x4:0x5]" &&
        error "command should have failed because of invalid syntax"
    rbh_find "rbh:$db:$testdb" -fid "[0x3:0x4:0x5]]" &&
        error "command should have failed because of invalid syntax"
    rbh_find "rbh:$db:$testdb" -fid "[0x3:0x4:0x5]invalid" &&
        error "command should have failed because of invalid syntax"
    rbh_find "rbh:$db:$testdb" -fid "[0x3:0x4:0x5)" &&
        error "command should have failed because of invalid syntax"
    rbh_find "rbh:$db:$testdb" -fid "]0x3:0x4:0x5[" &&
        error "command should have failed because of invalid syntax"
    rbh_find "rbh:$db:$testdb" -fid "[0x3:0x4:0x5][]" &&
        error "command should have failed because of invalid syntax"

    return 0
}

test_unknown_fid()
{
    touch "unknown_fid"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -fid 0x0:0x0:0x0 | sort |
        difflines
}

test_known_fid()
{
    local file="known_fid"

    touch "$file"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    local fid=$(lfs path2fid "$file")
    rbh_find "rbh:$db:$testdb" -fid "$fid" | sort |
        difflines "/$file"

    # remove braces around fid
    fid="${fid:1:-1}"
    rbh_find "rbh:$db:$testdb" -fid "$fid" | sort |
        difflines "/$file"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_syntax_error test_unknown_fid test_known_fid)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
