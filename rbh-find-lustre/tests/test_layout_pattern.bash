#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

if ! command -v rbh-sync &> /dev/null; then
    echo "This test requires rbh-sync to be installed" >&2
    exit 1
fi

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    rbh_lfind "rbh:mongo:$testdb" -layout-pattern 42 &&
        error "find with an invalid layout should have failed"

    rbh_lfind "rbh:mongo:$testdb" -layout-pattern invalid &&
        error "find with an invalid layout should have failed"

    return 0
}

test_default()
{
    local dir=test_default_layout

    mkdir $dir

    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -layout-pattern default | sort |
        difflines "/" "/$dir"

    lfs setstripe -L mdt -E 1M $dir
    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -layout-pattern default | sort |
        difflines "/"

    lfs setstripe -L raid0 $dir
    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -layout-pattern default | sort |
        difflines "/"
}

test_layout()
{
    local dir=test_layout_dir
    local file=test_layout

    mkdir $dir

    lfs setstripe -L raid0 -c 1 $dir/$file
    lfs setstripe -L raid0 -c 1 .
    lfs setstripe -L mdt -E 1M $dir

    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -layout-pattern raid0 | sort |
        difflines "/" "/$dir/$file"
    rbh_lfind "rbh:mongo:$testdb" -layout-pattern mdt | sort |
        difflines "/$dir"

    lfs migrate -L mdt -E 1M $dir/$file
    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -layout-pattern raid0 | sort |
        difflines "/"
    rbh_lfind "rbh:mongo:$testdb" -layout-pattern mdt | sort |
        difflines "/$dir" "/$dir/$file"

    lfs setstripe -L mdt -E 1M .
    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -layout-pattern raid0 | sort |
        difflines
    rbh_lfind "rbh:mongo:$testdb" -layout-pattern mdt | sort |
        difflines "/" "/$dir" "/$dir/$file"
}

test_default_layout()
{
    local dir=test_default_layout_dir
    local file=test_default_layout

    mkdir $dir
    lfs setstripe -L raid0 -c 1 $file

    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -layout-pattern mdt | sort |
        difflines

    lfs setstripe -L mdt -E 1M $LUSTRE_DIR

    rbh_lfind "rbh:mongo:$testdb" -layout-pattern mdt | sort |
        difflines "/" "/$dir"
    rbh_lfind "rbh:mongo:$testdb" -layout-pattern raid0 | sort |
        difflines "/$file"

    lfs setstripe -L raid0 -c 1 $LUSTRE_DIR

    rbh_lfind "rbh:mongo:$testdb" -layout-pattern mdt | sort |
        difflines
    rbh_lfind "rbh:mongo:$testdb" -layout-pattern raid0 | sort |
        difflines "/" "/$file" "/$dir"
}

test_other_layouts()
{
    local file1=test_layout_overstriped

    lfs setstripe -E 1M -C 5 -E -1 -c -1 $file1

    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -layout-pattern overstriped | sort |
        difflines "/$file1"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_default test_layout
                  test_default_layout test_other_layouts)

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap "rm -r $tmpdir; lfs setstripe -L raid0 $LUSTRE_DIR" EXIT

cd "$tmpdir"

run_tests "${tests[@]}"
