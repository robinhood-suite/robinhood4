#!/usr/bin/env bash

# This file is part of RobinHood 4.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/common_logs.bash

################################################################################
#                                    TESTS                                     #
################################################################################

log_should_exist()
{
    local id="$1"
    local error_msg="$2"

    if [ -z "$(do_db find_log $testdb $id)" ]; then
        error "$error_msg"
    fi
}

log_shouldnt_exist()
{
    local id="$1"
    local error_msg="$2"

    if [ ! -z "$(do_db find_log $testdb $id)" ]; then
        error "$error_msg"
    fi
}

test_delete()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    local id1="$(do_db get_last_log_id $testdb)"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    local id2="$(do_db get_last_log_id $testdb)"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    local id3="$(do_db get_last_log_id $testdb)"
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"
    local id4="$(do_db get_last_log_id $testdb)"

    rbh_find rbh:$db:$testdb > /dev/null
    local id5="$(do_db get_last_log_id $testdb)"
    rbh_find rbh:$db:$testdb > /dev/null
    local id6="$(do_db get_last_log_id $testdb)"

    rbh_report rbh:$db:$testdb \
        --group-by "statx.uid" \
        --output "sum(statx.size)" > /dev/null
    local id7="$(do_db get_last_log_id $testdb)"

    rbh_log rbh:$db:$testdb --log-count | sort |
        difflines "Log count for the 'find' command: '2'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '1'" \
                  "Log count for the 'sync' command: '4'" \
                  "Total log count: '7'"

    log_should_exist $id1
    log_should_exist $id2
    log_should_exist $id3
    log_should_exist $id4
    log_should_exist $id5
    log_should_exist $id6
    log_should_exist $id7

    rbh_log rbh:$db:$testdb --tool sync -n 3 --delete |
        difflines "Deleted '3' log(s)"

    rbh_log rbh:$db:$testdb --log-count | sort |
        difflines "Log count for the 'find' command: '2'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '1'" \
                  "Log count for the 'sync' command: '1'" \
                  "Total log count: '4'"

    log_should_exist $id1
    log_shouldnt_exist $id2
    log_shouldnt_exist $id3
    log_shouldnt_exist $id4
    log_should_exist $id5
    log_should_exist $id6
    log_should_exist $id7

    rbh_log rbh:$db:$testdb --tool gc -n 7 --delete |
        difflines "Deleted '0' log(s)"

    rbh_log rbh:$db:$testdb --log-count | sort |
        difflines "Log count for the 'find' command: '2'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '1'" \
                  "Log count for the 'sync' command: '1'" \
                  "Total log count: '4'"

    log_should_exist $id1
    log_shouldnt_exist $id2
    log_shouldnt_exist $id3
    log_shouldnt_exist $id4
    log_should_exist $id5
    log_should_exist $id6
    log_should_exist $id7

    rbh_log rbh:$db:$testdb -n 1 --delete |
        difflines "Deleted '1' log(s)"

    rbh_log rbh:$db:$testdb --log-count | sort |
        difflines "Log count for the 'find' command: '2'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '0'" \
                  "Log count for the 'sync' command: '1'" \
                  "Total log count: '3'"

    log_should_exist $id1
    log_shouldnt_exist $id2
    log_shouldnt_exist $id3
    log_shouldnt_exist $id4
    log_should_exist $id5
    log_should_exist $id6
    log_shouldnt_exist $id7

    rbh_log rbh:$db:$testdb -n 1 --delete |
        difflines "Deleted '1' log(s)"

    rbh_log rbh:$db:$testdb --log-count | sort |
        difflines "Log count for the 'find' command: '1'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '0'" \
                  "Log count for the 'sync' command: '1'" \
                  "Total log count: '2'"

    log_should_exist $id1
    log_shouldnt_exist $id2
    log_shouldnt_exist $id3
    log_shouldnt_exist $id4
    log_should_exist $id5
    log_shouldnt_exist $id6
    log_shouldnt_exist $id7

    rbh_log rbh:$db:$testdb --tool "find,report" -n 7 --delete |
        difflines "Deleted '1' log(s)"

    rbh_log rbh:$db:$testdb --log-count | sort |
        difflines "Log count for the 'find' command: '0'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '0'" \
                  "Log count for the 'sync' command: '1'" \
                  "Total log count: '1'"

    log_should_exist $id1
    log_shouldnt_exist $id2
    log_shouldnt_exist $id3
    log_shouldnt_exist $id4
    log_shouldnt_exist $id5
    log_shouldnt_exist $id6
    log_shouldnt_exist $id7

    rbh_log rbh:$db:$testdb -n 42 --delete |
        difflines "Deleted '1' log(s)"

    rbh_log rbh:$db:$testdb --log-count | sort |
        difflines "Log count for the 'find' command: '0'" \
                  "Log count for the 'fsevents' command: '0'" \
                  "Log count for the 'gc' command: '0'" \
                  "Log count for the 'report' command: '0'" \
                  "Log count for the 'sync' command: '0'" \
                  "Total log count: '0'"

    log_shouldnt_exist $id1
    log_shouldnt_exist $id2
    log_shouldnt_exist $id3
    log_shouldnt_exist $id4
    log_shouldnt_exist $id5
    log_shouldnt_exist $id6
    log_shouldnt_exist $id7
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_delete)

tmpdir=$(mktemp --directory)
trap -- "rm -rf '$tmpdir'" EXIT
cd "$tmpdir"

run_tests "${tests[@]}"
