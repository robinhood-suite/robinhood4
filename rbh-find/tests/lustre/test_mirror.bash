#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_mirror_count()
{
    mongo_only_test

    touch file
    lfs mirror create -N2 file2
    lfs mirror create -N3 file3

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -mirror-count 2 | sort |
        difflines "/file2"
    rbh_find "rbh:$db:$testdb" -mirror-count +2 | sort |
        difflines "/file3"
    rbh_find "rbh:$db:$testdb" -mirror-count -2 | sort |
        difflines
    rbh_find "rbh:$db:$testdb" -not -mirror-count 3 | sort |
        difflines "/" "/file" "/file2"
}

test_invalid_mirror_state()
{
    rbh_find "rbh:$db:$testdb" -mirror-state 42 &&
        error "rbh-find with an invalid mirror state should have failed"

    rbh_find "rbh:$db:$testdb" -mirror-state invalid &&
        error "rbh-find with an invalid mirror state should have failed"

    return 0
}

test_mirror_state()
{
    # File with mirror have the read-only state by default. If we write on a
    # file, its state will now be write pending until a resync is done. After,
    # the resync, its state will be read-only again.

    # Should be read-only
    lfs mirror create -N2 file

    # Should be write pending
    lfs mirror create -N3 file2
    echo "foo" > file2

    # Should have nothing as it's not a mirrored file
    lfs setstripe -E 1M -c 1 -S 256k -E 512M -c 2 -S 512k -E -1 -c -1 -S 1024k \
        "file_comp"
    truncate -s 1M "file_comp"
    echo "foo" > file_comp

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -mirror-state ro | sort | difflines "/file"
    rbh_find "rbh:$db:$testdb" -mirror-state wp | sort | difflines "/file2"

    # Should be read-only now
    lfs mirror resync file2

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -mirror-state ro | sort |
        difflines "/file" "/file2"
    rbh_find "rbh:$db:$testdb" -mirror-state wp | sort | difflines
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_mirror_count test_invalid_mirror_state test_mirror_state)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
