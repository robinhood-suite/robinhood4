#!/usr/bin/env bash

# This file is part of RobinHood 4.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/common_stats.bash

################################################################################
#                                    TESTS                                     #
################################################################################

stats_should_exist()
{
    local id="$1"
    local error_msg="$2"

    if [ -z "$(do_db find_stats $testdb $id)" ]; then
        error "$error_msg"
    fi
}

stats_shouldnt_exist()
{
    local id="$1"
    local error_msg="$2"

    >&2 echo "$LINENO"
    if [ ! -z "$(do_db find_stats $testdb $id)" ]; then
        error "$error_msg"
    fi
}

test_delete()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    local id1="$(do_db get_last_stats_id $testdb)"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    local id2="$(do_db get_last_stats_id $testdb)"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    local id3="$(do_db get_last_stats_id $testdb)"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    local id4="$(do_db get_last_stats_id $testdb)"

    rbh_find rbh:$db:$testdb > /dev/null
    local id5="$(do_db get_last_stats_id $testdb)"
    rbh_find rbh:$db:$testdb > /dev/null
    local id6="$(do_db get_last_stats_id $testdb)"

    rbh_report rbh:$db:$testdb \
        --group-by "statx.uid" \
        --output "sum(statx.size)" > /dev/null
    local id7="$(do_db get_last_stats_id $testdb)"

    rbh_stats rbh:$db:$testdb --stats-count | sort |
        difflines "Log count for the 'find' command: '2'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '1'" \
                  "Log count for the 'sync' command: '4'" \
                  "Total stats count: '7'"

    stats_should_exist $id1 "Should have found '$id1'"
    stats_should_exist $id2 "Should have found '$id2'"
    stats_should_exist $id3 "Should have found '$id3'"
    stats_should_exist $id4 "Should have found '$id4'"
    stats_should_exist $id5 "Should have found '$id5'"
    stats_should_exist $id6 "Should have found '$id6'"
    stats_should_exist $id7 "Should have found '$id7'"

    rbh_stats rbh:$db:$testdb --tool sync -n 3 --delete |
        difflines "Deleted '3' stats(s)"

    rbh_stats rbh:$db:$testdb --stats-count | sort |
        difflines "Log count for the 'find' command: '2'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '1'" \
                  "Log count for the 'sync' command: '1'" \
                  "Total stats count: '4'"

    stats_should_exist $id1 "Should have found '$id1'"
    stats_shouldnt_exist $id2 "Shouldnt have found '$id2'"
    stats_shouldnt_exist $id3 "Shouldnt have found '$id3'"
    stats_shouldnt_exist $id4 "Shouldnt have found '$id4'"
    stats_should_exist $id5 "Should have found '$id5'"
    stats_should_exist $id6 "Should have found '$id6'"
    stats_should_exist $id7 "Should have found '$id7'"

    rbh_stats rbh:$db:$testdb --tool gc -n 7 --delete |
        difflines "Deleted '0' stats(s)"

    rbh_stats rbh:$db:$testdb --stats-count | sort |
        difflines "Log count for the 'find' command: '2'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '1'" \
                  "Log count for the 'sync' command: '1'" \
                  "Total stats count: '4'"

    stats_should_exist $id1 "Should have found '$id1'"
    stats_shouldnt_exist $id2 "Shouldnt have found '$id2'"
    stats_shouldnt_exist $id3 "Shouldnt have found '$id3'"
    stats_shouldnt_exist $id4 "Shouldnt have found '$id4'"
    stats_should_exist $id5 "Should have found '$id5'"
    stats_should_exist $id6 "Should have found '$id6'"
    stats_should_exist $id7 "Should have found '$id7'"

    rbh_stats rbh:$db:$testdb -n 1 --delete |
        difflines "Deleted '1' stats(s)"

    rbh_stats rbh:$db:$testdb --stats-count | sort |
        difflines "Log count for the 'find' command: '2'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '0'" \
                  "Log count for the 'sync' command: '1'" \
                  "Total stats count: '3'"

    stats_should_exist $id1 "Should have found '$id1'"
    stats_shouldnt_exist $id2 "Shouldnt have found '$id2'"
    stats_shouldnt_exist $id3 "Shouldnt have found '$id3'"
    stats_shouldnt_exist $id4 "Shouldnt have found '$id4'"
    stats_should_exist $id5 "Should have found '$id5'"
    stats_should_exist $id6 "Should have found '$id6'"
    stats_shouldnt_exist $id7 "Shouldnt have found '$id7'"

    rbh_stats rbh:$db:$testdb -n 1 --delete |
        difflines "Deleted '1' stats(s)"

    rbh_stats rbh:$db:$testdb --stats-count | sort |
        difflines "Log count for the 'find' command: '1'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '0'" \
                  "Log count for the 'sync' command: '1'" \
                  "Total stats count: '2'"

    stats_should_exist $id1 "Should have found '$id1'"
    stats_shouldnt_exist $id2 "Shouldnt have found '$id2'"
    stats_shouldnt_exist $id3 "Shouldnt have found '$id3'"
    stats_shouldnt_exist $id4 "Shouldnt have found '$id4'"
    stats_should_exist $id5 "Should have found '$id5'"
    stats_shouldnt_exist $id6 "Shouldnt have found '$id6'"
    stats_shouldnt_exist $id7 "Shouldnt have found '$id7'"

    rbh_stats rbh:$db:$testdb --tool "find,report" -n 7 --delete |
        difflines "Deleted '1' stats(s)"

    rbh_stats rbh:$db:$testdb --stats-count | sort |
        difflines "Log count for the 'find' command: '0'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '0'" \
                  "Log count for the 'sync' command: '1'" \
                  "Total stats count: '1'"

    stats_should_exist $id1 "Should have found '$id1'"
    stats_shouldnt_exist $id2 "Shouldnt have found '$id2'"
    stats_shouldnt_exist $id3 "Shouldnt have found '$id3'"
    stats_shouldnt_exist $id4 "Shouldnt have found '$id4'"
    stats_shouldnt_exist $id5 "Shouldnt have found '$id5'"
    stats_shouldnt_exist $id6 "Shouldnt have found '$id6'"
    stats_shouldnt_exist $id7 "Shouldnt have found '$id7'"

    rbh_stats rbh:$db:$testdb -n 42 --delete |
        difflines "Deleted '1' stats(s)"

    rbh_stats rbh:$db:$testdb --stats-count | sort |
        difflines "Log count for the 'find' command: '0'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '0'" \
                  "Log count for the 'sync' command: '0'" \
                  "Total stats count: '0'"

    stats_shouldnt_exist $id1 "Shouldnt have found '$id1'"
    stats_shouldnt_exist $id2 "Shouldnt have found '$id2'"
    stats_shouldnt_exist $id3 "Shouldnt have found '$id3'"
    stats_shouldnt_exist $id4 "Shouldnt have found '$id4'"
    stats_shouldnt_exist $id5 "Shouldnt have found '$id5'"
    stats_shouldnt_exist $id6 "Shouldnt have found '$id6'"
    stats_shouldnt_exist $id7 "Shouldnt have found '$id7'"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_delete)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests "${tests[@]}"
