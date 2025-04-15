#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifer: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    rbh_find "rbh:mongo:$testdb" -stripe-count $(echo 2^64 | bc) &&
        error "find with an ost index too big should have failed"

    rbh_find "rbh:mongo:$testdb" -stripe-count 42blob &&
        error "find with an invalid ost index should have failed"

    rbh_find "rbh:mongo:$testdb" -stripe-count invalid &&
        error "find with an invalid ost index should have failed"

    return 0
}

test_default()
{
    local dir=test_default_dir

    local default_stripe_count="$(lfs getstripe -c $LUSTRE_DIR | xargs)"
    mkdir $dir

    rbh_sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -stripe-count default | sort |
        difflines "/" "/$dir"

    lfs setstripe -c $((default_stripe_count + 1)) $dir
    rbh_sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -stripe-count default | sort |
        difflines "/"

    lfs setstripe -c $((default_stripe_count)) $dir
    rbh_sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -stripe-count default | sort |
        difflines "/"
}

test_stripe_count()
{
    local dir=test_stripe_count_dir
    local file=test_stripe_count

    lfs mkdir $dir

    lfs setstripe -c 1 $dir/$file
    lfs setstripe -c 2 $dir
    lfs setstripe -c 3 .

    rbh_sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -stripe-count 1 | sort |
        difflines "/$dir/$file"
    rbh_find "rbh:mongo:$testdb" -stripe-count 2 | sort |
        difflines "/$dir"
    rbh_find "rbh:mongo:$testdb" -stripe-count 3 | sort |
        difflines "/"

    rbh_find "rbh:mongo:$testdb" -stripe-count -4 | sort |
        difflines "/" "/$dir" "/$dir/$file"
    rbh_find "rbh:mongo:$testdb" -stripe-count -3 | sort |
        difflines "/$dir" "/$dir/$file"
    rbh_find "rbh:mongo:$testdb" -stripe-count -2 | sort |
        difflines "/$dir/$file"

    rbh_find "rbh:mongo:$testdb" -stripe-count +0 | sort |
        difflines "/" "/$dir" "/$dir/$file"
    rbh_find "rbh:mongo:$testdb" -stripe-count +1 | sort |
        difflines "/" "/$dir"
    rbh_find "rbh:mongo:$testdb" -stripe-count +2 | sort |
        difflines "/"

    lfs migrate -c 3 $dir/$file
    rbh_sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -stripe-count 3 | sort |
        difflines "/" "/$dir/$file"

    lfs setstripe -c 3 $dir
    rbh_sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" -stripe-count 3 | sort |
        difflines "/" "/$dir" "/$dir/$file"

    rbh_find "rbh:mongo:$testdb" -stripe-count +2 | sort |
        difflines "/" "/$dir" "/$dir/$file"
    rbh_find "rbh:mongo:$testdb" -stripe-count -4 | sort |
        difflines "/" "/$dir" "/$dir/$file"
}

test_default_stripe_count()
{
    local dir=test_default_stripe_count_dir
    local file=test_default_stripe_count

    lfs mkdir $dir
    lfs setstripe -c 4 $file

    rbh_sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_find "rbh:mongo:$testdb" \
        -stripe-count $(lfs getstripe -c $LUSTRE_DIR | xargs) | sort |
        difflines

    lfs setstripe -c 3 $LUSTRE_DIR

    rbh_find "rbh:mongo:$testdb" -stripe-count 3 | sort |
        difflines "/" "/$dir"

    rbh_find "rbh:mongo:$testdb" -stripe-count -3 | sort |
        difflines
    rbh_find "rbh:mongo:$testdb" -stripe-count -5 | sort |
        difflines "/" "/$file" "/$dir"
    rbh_find "rbh:mongo:$testdb" -stripe-count +3 | sort |
        difflines "/$file"
    rbh_find "rbh:mongo:$testdb" -stripe-count +2 | sort |
        difflines "/" "/$file" "/$dir"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_default test_stripe_count
                  test_default_stripe_count)

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap "rm -r $tmpdir; lfs setstripe -c 0 $LUSTRE_DIR" EXIT

cd "$tmpdir"

run_tests "${tests[@]}"
