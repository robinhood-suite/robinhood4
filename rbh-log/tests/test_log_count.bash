#!/usr/bin/env bash

# This file is part of RobinHood.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/common_logs.bash
. $test_dir/lustre_utils.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_log_count()
{
    local sync_count=0
    local find_count=0
    local fsevents_count=0
    local report_count=0
    local gc_count=0

    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    sync_count=$((sync_count + 1))

    # Output 20 random ints between 0 and 4
    for i in $(shuf -i 0-4 -r -n 20); do
        case "$i" in
            0)
                rbh_sync rbh:posix:. rbh:$db:$testdb
                sync_count=$((sync_count + 1))
                ;;
            1)
                rbh_find rbh:$db:$testdb -exec ls \; > /dev/null
                find_count=$((find_count + 1))
                ;;
            2)
                rbh_fsevents --enrich rbh:lustre:$LUSTRE_DIR \
                    src:lustre:$LUSTRE_MDT rbh:$db:$testdb > /dev/null
                fsevents_count=$((fsevents_count + 1))
                ;;
            3)
                rbh_report rbh:$db:$testdb \
                    --group-by "statx.uid" \
                    --output "sum(statx.size)" > /dev/null
                report_count=$((report_count + 1))
                ;;
            4)
                rbh_gc rbh:$db:$testdb --sync-time 42
                gc_count=$((gc_count + 1))
                ;;
        esac
    done

    local total_count=$((find_count + fsevents_count + gc_count +
                         report_count + sync_count))

    rbh_log rbh:$db:$testdb --count | sort |
        difflines "Log count for the 'find' command: '$find_count'" \
                  "Log count for the 'fsevents' command: '$fsevents_count'" \
                  "Log count for the 'gc' command: '$gc_count'" \
                  "Log count for the 'report' command: '$report_count'" \
                  "Log count for the 'sync' command: '$sync_count'" \
                  "Total log count: '$total_count'"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_log_count)

LUSTRE_DIR=/mnt/lustre/
cd "$LUSTRE_DIR"

LUSTRE_MDT=lustre-MDT0000
userid="$(start_changelogs "$LUSTRE_MDT")"

tmpdir=$(mktemp --directory --tmpdir=$LUSTRE_DIR)
lfs setdirstripe -D -i 0 $tmpdir
trap -- "rm -rf '$tmpdir'; stop_changelogs '$LUSTRE_MDT' '$userid'" EXIT
cd "$tmpdir"

sub_setup=lustre_setup
sub_teardown=lustre_teardown
run_tests "${tests[@]}"
