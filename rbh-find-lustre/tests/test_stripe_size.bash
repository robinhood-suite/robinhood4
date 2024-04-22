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
    rbh_lfind "rbh:mongo:$testdb" -stripe-size $(echo 2^64 | bc) &&
        error "find with a stripe size too big should have failed"

    rbh_lfind "rbh:mongo:$testdb" -stripe-size 42blob &&
        error "find with an invalid stripe size should have failed"

    rbh_lfind "rbh:mongo:$testdb" -stripe-size invalid &&
        error "find with an invalid stripe size should have failed"

    return 0
}

test_default()
{
    local dir=test_default_dir

    local default_stripe_size="$(lfs getstripe -S $LUSTRE_DIR | xargs)"
    mkdir $dir

    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -stripe-size default | sort |
        difflines "/" "/$dir"

    lfs setstripe -S $((default_stripe_size << 1)) $dir
    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -stripe-size default | sort |
        difflines "/"

    lfs setstripe -S $((default_stripe_size)) $dir
    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -stripe-size default | sort |
        difflines "/"
}

test_stripe_size()
{
    local dir=test_stripe_size_dir
    local file=test_stripe_size

    lfs mkdir $dir

    lfs setstripe -S 1M $dir/$file
    lfs setstripe -S 2M $dir
    lfs setstripe -S 3M .

    local oneM="$(lfs getstripe -S $dir/$file | xargs)"
    local twoM="$(($oneM * 2))"
    local threeM="$(($oneM * 3))"
    local fourM="$(($oneM * 4))"

    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -stripe-size $oneM | sort |
        difflines "/$dir/$file"
    rbh_lfind "rbh:mongo:$testdb" -stripe-size $twoM | sort |
        difflines "/$dir"
    rbh_lfind "rbh:mongo:$testdb" -stripe-size $threeM | sort |
        difflines "/"

    rbh_lfind "rbh:mongo:$testdb" -stripe-size -$fourM | sort |
        difflines "/" "/$dir" "/$dir/$file"
    rbh_lfind "rbh:mongo:$testdb" -stripe-size -$((threeM - 1)) | sort |
        difflines "/$dir" "/$dir/$file"
    rbh_lfind "rbh:mongo:$testdb" -stripe-size -$twoM | sort |
        difflines "/$dir/$file"

    rbh_lfind "rbh:mongo:$testdb" -stripe-size +0 | sort |
        difflines "/" "/$dir" "/$dir/$file"
    rbh_lfind "rbh:mongo:$testdb" -stripe-size +$oneM | sort |
        difflines "/" "/$dir"
    rbh_lfind "rbh:mongo:$testdb" -stripe-size +$twoM | sort |
        difflines "/"

    lfs migrate -S $threeM $dir/$file
    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -stripe-size $threeM | sort |
        difflines "/" "/$dir/$file"

    lfs setstripe -S $threeM $dir
    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" -stripe-size $threeM | sort |
        difflines "/" "/$dir" "/$dir/$file"

    rbh_lfind "rbh:mongo:$testdb" -stripe-size +$twoM | sort |
        difflines "/" "/$dir" "/$dir/$file"
    rbh_lfind "rbh:mongo:$testdb" -stripe-size -$fourM | sort |
        difflines "/" "/$dir" "/$dir/$file"
}

test_default_stripe_size()
{
    local dir=test_default_stripe_size_dir
    local file=test_default_stripe_size

    lfs mkdir $dir
    lfs setstripe -S 4M $file

    local fourM="$(lfs getstripe -S $file | xargs)"
    local threeM="$(($fourM * 3 / 4))"
    local twoM="$(($fourM >> 1))"

    rbh-sync "rbh:lustre:." "rbh:mongo:$testdb"

    rbh_lfind "rbh:mongo:$testdb" \
        -stripe-size $(lfs getstripe -S $LUSTRE_DIR | xargs) | sort |
        difflines

    lfs setstripe -S $threeM $LUSTRE_DIR

    rbh_lfind "rbh:mongo:$testdb" -stripe-size $threeM | sort |
        difflines "/" "/$dir"

    rbh_lfind "rbh:mongo:$testdb" -stripe-size -$threeM | sort |
        difflines
    rbh_lfind "rbh:mongo:$testdb" -stripe-size -$(($fourM << 1)) | sort |
        difflines "/" "/$file" "/$dir"
    rbh_lfind "rbh:mongo:$testdb" -stripe-size +$threeM | sort |
        difflines "/$file"
    rbh_lfind "rbh:mongo:$testdb" -stripe-size +$twoM | sort |
        difflines "/" "/$file" "/$dir"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_default test_stripe_size
                  test_default_stripe_size)

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap "rm -r $tmpdir; lfs setstripe -S 0 $LUSTRE_DIR" EXIT

cd "$tmpdir"

run_tests "${tests[@]}"
