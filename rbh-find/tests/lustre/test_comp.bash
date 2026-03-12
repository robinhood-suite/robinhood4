#!/usr/bin/env bash

# This file is part of RobinHood
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../../utils/tests/framework.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_comp_start()
{
    local file="test_comp_start"

    lfs setstripe -E 1M -c 1 -S 256k -E 512M -c 2 -S 512k -E -1 -c -1 -S 1024k \
        "$file"
    truncate -s 1M "$file"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -comp-start -1k | sort |
        difflines "/$file"
    rbh_find "rbh:$db:$testdb" -comp-start +1k | sort |
        difflines "/$file"
    rbh_find "rbh:$db:$testdb" -comp-start 1023k -or -comp-start 1k | sort |
        difflines
    rbh_find "rbh:$db:$testdb" -comp-start -2T | sort |
        difflines "/$file"
    rbh_find "rbh:$db:$testdb" -comp-start 1M -comp-start 512M | sort |
        difflines "/$file"
    rbh_find "rbh:$db:$testdb" -comp-start 1M,512M | sort |
        difflines "/$file"
    rbh_find "rbh:$db:$testdb" -comp-start 1M -comp-start 511M | sort |
        difflines
    rbh_find "rbh:$db:$testdb" -comp-start 1M,511M | sort |
        difflines "/$file"
    rbh_find "rbh:$db:$testdb" -not '(' -comp-start 1M -comp-start 511M ')' |
        sort | difflines "/" "/$file"
}

test_comp_end()
{
    local file="test_comp_end"

    lfs setstripe -E 1M -c 1 -S 256k -E 512M -c 2 -S 512k -E -1 -c -1 -S 1024k \
        "$file"
    truncate -s 1M "$file"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -comp-end -1k | sort |
        difflines
    rbh_find "rbh:$db:$testdb" -comp-end +1k | sort |
        difflines "/$file"
    rbh_find "rbh:$db:$testdb" -comp-end 1023k -or -comp-end 1k | sort |
        difflines
    rbh_find "rbh:$db:$testdb" -comp-end -2T | sort |
        difflines "/$file"
    rbh_find "rbh:$db:$testdb" -comp-end 1M -comp-end 512M | sort |
        difflines "/$file"
    rbh_find "rbh:$db:$testdb" -comp-end 1M,512M | sort |
        difflines "/$file"
    rbh_find "rbh:$db:$testdb" -comp-end 1M -comp-end 511M | sort |
        difflines
    rbh_find "rbh:$db:$testdb" -comp-end 1M,511M | sort |
        difflines "/$file"
    rbh_find "rbh:$db:$testdb" -not '(' -comp-end 1M -comp-end 511M ')' |
        sort | difflines "/" "/$file"
}

test_comp_count()
{
    mongo_only_test

    touch 1comp
    lfs setstripe -E 1M -c 1 -S 256k -E 512M -c 2 -S 512k -E -1 -c -1 -S 1024k \
        "3comp"
    truncate -s 1M "3comp"

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -comp-count 3 | sort |
        difflines "/3comp"
    rbh_find "rbh:$db:$testdb" -comp-count -3 | sort |
        difflines "/1comp"
    rbh_find "rbh:$db:$testdb" -comp-count +0 | sort |
        difflines "/1comp" "/3comp"
    rbh_find "rbh:$db:$testdb" -not -comp-count 3 | sort |
        difflines "/" "/1comp"
}

test_comp_flags()
{
    lfs setstripe -E 1M -S 256k -E 2M -S 1M -E -1 -S 1G file0
    truncate -s 1M "file0"

    # XXX: extension components don't have a stripe size but an extension size
    #lfs setstripe --extension-size 64M -c 1 -E -1 file1

    lfs mirror create -N -Eeof -c2 -o0,1 -N -Eeof -c2 -o1,2 file2
    echo "blob" > file2

    lfs setstripe -E 1M -S 256k -E 2M -S 1M -E -1 -S 1G file3
    lfs setstripe --comp-set -I 1 --comp-flags=nosync file3

    lfs setstripe -E 1M -S 256k -E -1 -S 1M file4
    lfs setstripe --comp-set -I 1 --comp-flags=prefer file4

    rbh_sync "rbh:lustre:." "rbh:$db:$testdb"

    rbh_find "rbh:$db:$testdb" -comp-flags init | sort |
        difflines "/file0" "/file2" "/file3" "/file4"
    #rbh_find "rbh:$db:$testdb" -comp-flags extension | sort |
    #    difflines "/file1"
    rbh_find "rbh:$db:$testdb" -comp-flags stale | sort |
        difflines "/file2"
    rbh_find "rbh:$db:$testdb" -comp-flags nosync | sort |
        difflines "/file3"
    rbh_find "rbh:$db:$testdb" -comp-flags prefer | sort |
        difflines "/file4"

    rbh_find "rbh:$db:$testdb" -comp-flags init -comp-flags prefer| sort |
        difflines "/file4"
    rbh_find "rbh:$db:$testdb" -comp-flags stale -or -comp-flags prefer| sort |
        difflines "/file2" "/file4"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_comp_start test_comp_end test_comp_count test_comp_flags)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
