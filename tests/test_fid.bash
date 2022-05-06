#!/usr/bin/env bash

# This file is part of rbh-find-lustre
# Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

set -xe

if ! command -v rbh-sync &> /dev/null; then
    echo "This test requires rbh-sync to be installed" >&2
    exit 1
fi

if ! lctl get_param mdt.*.hsm_control | grep "enabled"; then
    echo "At least 1 MDT needs to have HSM control enabled" >&2
    exit 1
fi

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

retrieve_fid()
{
    local file="$1"
    local fid

    fid=$(mongo $testdb --quiet \
        --eval "db.entries.find({'ns.name': '$file'},
                                {_id: 0, 'ns.xattrs.fid': 1})")

    # The mongo output will be something like
    # '{ "ns" : [ { "xattrs" : { "fid" : "0x200000401:0x210:0x0" } } ] }'
    fid=$(echo "$fid" | cut -d'"' -f8)
    echo "$fid"
}

################################################################################
#                                    TESTS                                     #
################################################################################

test_syntax_error()
{
    touch "dummy"
    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -fid fail &&
        error "command should have failed because of invalid syntax"
    rbh_lfind "rbh:mongo:$testdb" -fid 0x3 &&
        error "command should have failed because of invalid syntax"
    rbh_lfind "rbh:mongo:$testdb" -fid 0x3:0x4 &&
        error "command should have failed because of invalid syntax"

    return 0
}

test_unknown_fid()
{
    touch "unknown_fid"
    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -fid 0x0:0x0:0x0 | sort |
        difflines
}

test_known_fid()
{
    local file="known_fid"

    touch "$file"
    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    local fid=$(retrieve_fid "$file")

    rbh_lfind "rbh:mongo:$testdb" -fid "$fid" | sort |
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
