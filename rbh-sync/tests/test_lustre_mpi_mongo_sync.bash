#!/usr/bin/env bash

# This file is part of RobinHood 4
# Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/test_utils.bash
. $test_dir/test_lustre_utils.bash

################################################################################
#                                     MAIN                                     #
################################################################################

export USE_MPI=true
declare -a tests=(test_simple_sync test_branch_sync)

if lctl get_param mdt.*.hsm_control | grep "enabled"; then
    tests+=(test_hsm_state_none test_hsm_state_archived_states
            test_hsm_state_independant_states test_hsm_state_multiple_states
            test_hsm_archive_id)
fi

tests+=(test_flags test_gen test_mirror_count test_stripe_count
        test_stripe_size test_pattern test_comp_flags test_pool test_mirror_id
        test_begin test_end test_ost test_mdt_index_file test_mdt_index_dir
        test_mdt_hash test_mdt_count)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests ${tests[@]}
