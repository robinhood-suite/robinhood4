#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

# Depending on the libfabric's version and OS, libfabric can have network errors
# with PSM3. To solve this, we specify the PSM3 devices as below.
# https://github.com/easybuilders/easybuild-easyconfigs/issues/18925
export PSM3_DEVICES="self"

rbh_sync_sparse()
{
    if [[ "$WITH_MPI" == "true" ]]; then
        rbh_sync "rbh:sparse-mpi:$1" "$2" $3
    else
        rbh_sync "rbh:sparse:$1" "$2" $3
    fi
}

################################################################################
#                                    TESTS                                     #
################################################################################

test_sparseness()
{
    local file0="non_sparse"
    local file1="sparse"
    local file2="empty"

    touch $file0 $file1 $file2
    bs0=$(stat -c "%o" "$file0")
    bs1=$(stat -c "%o" "$file1")

    dd oflag=direct conv=notrunc if=/dev/urandom of="$file0" \
        bs=$bs0 count=4

    dd oflag=direct conv=notrunc if=/dev/urandom of="$file1" \
        bs=$bs1 count=1
    dd oflag=direct conv=notrunc if=/dev/urandom of="$file1" \
        bs=$bs1 count=1 seek=2

    rbh_sync_sparse "." "rbh:$db:$testdb"

    local sparse1=$(bc <<< "$(find . -name "$file1" -printf "%S") * 100")
    sparse1="${sparse1%.*}"

    find_attribute "\"ns.xattrs.path\": \"/$file0\"" \
        "\"xattrs.sparseness\": 100"
    find_attribute "\"ns.xattrs.path\": \"/$file1\"" \
        "\"xattrs.sparseness\": $sparse1"
    find_attribute "\"ns.xattrs.path\": \"/$file2\"" \
        "\"xattrs.sparseness\": 100"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_sparseness)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
