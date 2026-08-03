#!/usr/bin/env bash

# This file is part of RobinHood.
# Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
#                    alternatives
#
# SPDX-License-Identifier: LGPL-3.0-or-later

test_dir=$(dirname $(readlink -e $0))
. $test_dir/../../utils/tests/framework.bash
. $test_dir/lustre_utils.bash
. $test_dir/common_logs.bash

################################################################################
#                                    TESTS                                     #
################################################################################

test_invalid()
{
    rbh_sync "rbh:posix:." "rbh:$db:$testdb"

    rbh_log "rbh:$db:$testdb" --fsevents blob &&
        error "log with invalid fsevents count should have failed"

    rbh_log "rbh:$db:$testdb" --fsevents 42invalid &&
        error "log with invalid fsevents count should have failed"

    return 0
}

check_log_result()
{
    local output="$1"

    check_common_logs "$output" rbh-fsevents \
        "rbh-fsevents --enrich rbh:lustre:$LUSTRE_DIR src:lustre:$LUSTRE_MDT rbh:$db:$testdb"

    echo "$output" | grep "Enrichment" > /dev/null ||
        error "enrich_mountpoint should have been retrieved, got '$output'"

    echo "$output" | grep "Source" > /dev/null ||
        error "source_read should have been retrieved, got '$output'"

    echo "$output" | grep "Number" > /dev/null ||
        error "worker_count should have been retrieved, got '$output'"

    echo "$output" | grep "Amount" > /dev/null ||
        error "changelog_read should have been retrieved, got '$output'"

    echo "$output" | grep "Starting" > /dev/null ||
        error "start_index should have been retrieved, got '$output'"

    echo "$output" | grep "reading/deduplicating" > /dev/null ||
        error "time_read_dedup should have been retrieved, got '$output'"

    echo "$output" | grep "enriching/updating" > /dev/null ||
        error "time_enrich_update should have been retrieved, got '$output'"

    echo "$output" | grep "skipped" > /dev/null ||
        error "enrich_skip_count should have been retrieved, got '$output'"

    echo "$output" | grep "Ratio" > /dev/null ||
        error "deduplication_ratio should have been retrieved, got '$output'"
}

test_N_logs()
{
    local requested=$1
    local expected=$2
    local count=15

    touch blob
    for i in $(seq 1 $expected); do
        invoke_rbh-fsevents
    done

    local output=$(rbh_log "rbh:$db:$testdb" --fsevents $requested)

    local n_lines=$(echo "$output" | wc -l)

    if ((n_lines != $count * $expected)); then
        error "There should be $count * $expected lines about fsevents ($(( $count - 2)) for content, 2 for header/footer, time $expected logs), got '$output'"
    fi

    for i in $(seq 1 $expected); do
        local one_log="$(echo "$output" | head -n $count)"
        check_log_result "$one_log"
        output="$(echo "$output" | sed 1,${count}d)"
    done
}

test_fsevents_1()
{
    test_N_logs 1 1
}

test_fsevents_N()
{
    test_N_logs 3 3
}

test_more_than_N()
{
    test_N_logs 6 3
}

test_timestamps()
{
    check_common_timestamps "--fsevents" \
        "rbh_fsevents --enrich rbh:lustre:$LUSTRE_DIR
         src:lustre:$LUSTRE_MDT rbh:$db:$testdb"
}

test_command_line()
{
    local conf="conf"
    local file="file"
    local dir="dir"
    local command="rbh-fsevents"

    echo "---
 mongo:
     address: \"mongodb://localhost:27017\"
---" > $conf

    touch file

    # These two commands will fail because we can't push events to the DB
    # without enriching them first. But since we don't care about these commands
    # actually suceeding, just keep them as-is just for proper testing of the
    # logs
    set +e
    rbh_fsevents src:lustre:$LUSTRE_MDT rbh:$db:$testdb > /dev/null
    rbh_fsevents --config $conf src:lustre:$LUSTRE_MDT rbh:$db:$testdb > /dev/null
    set -e
    rbh_fsevents --enrich rbh:lustre:$LUSTRE_DIR src:lustre:$LUSTRE_MDT rbh:$db:$testdb > /dev/null
    rbh_fsevents --enrich rbh:lustre:$LUSTRE_DIR src:lustre:$LUSTRE_MDT rbh:$db:$testdb -b 400 -d blob --no-estale-logs > /dev/null
    rbh_fsevents src:lustre:$LUSTRE_MDT - > tmp
    cat tmp | rbh_fsevents --enrich rbh:lustre:$LUSTRE_DIR - - > tmp
    cat tmp | rbh_fsevents - rbh:$db:$testdb > /dev/null

    rbh_log rbh:$db:$testdb --fsevents 7 | grep "Command" | cut -d':' -f2- |
        sed 's/^[ \t]*//' | sed -n "s/.*$command/$command/p" |
        # Two of the commands above are not here because their output was a file
        # and not the database, so there is no log associated
        difflines "rbh-fsevents - rbh:$db:$testdb" \
                  "rbh-fsevents --enrich rbh:lustre:$LUSTRE_DIR src:lustre:$LUSTRE_MDT rbh:$db:$testdb -b 400 -d blob --no-estale-logs" \
                  "rbh-fsevents --enrich rbh:lustre:$LUSTRE_DIR src:lustre:$LUSTRE_MDT rbh:$db:$testdb" \
                  "rbh-fsevents --config $conf src:lustre:$LUSTRE_MDT rbh:$db:$testdb" \
                  "rbh-fsevents src:lustre:$LUSTRE_MDT rbh:$db:$testdb"
}

test_source_and_enrichment()
{
    local other_mdt="lustre-MDT0001"
    local other_mdt_user=$(start_changelogs "$other_mdt")
    local entry="test_entry"

    # Clear changelogs from previous tests
    clear_changelogs "$LUSTRE_MDT" "$userid"
    clear_changelogs "$other_mdt" "$other_mdt_user"

    mkdir $entry
    lfs migrate -m 1 $entry
    lfs migrate -m 0 $entry

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb"

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" src:lustre:"$other_mdt" \
        "rbh:$db:$testdb"
    set +e
    rbh_fsevents --enrich rbh:lustre:. src:lustre:"$other_mdt" \
        "rbh:$db:$testdb"
    set -e

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb"

    stop_changelogs "$other_mdt" "$other_mdt_user"

    rbh_log rbh:$db:$testdb --fsevents 4 | grep "Source of the events" |
        cut -d':' -f2- | sed 's/^[ \t]*//' |
        difflines "$LUSTRE_MDT" "$other_mdt" "$other_mdt" "$LUSTRE_MDT"

    # LUSTRE_DIR without last slash
    local ldwls="${LUSTRE_DIR::-1}"
    rbh_log rbh:$db:$testdb --fsevents 4 | grep "Enrichment mountpoint" |
        cut -d':' -f2- | sed 's/^[ \t]*//' |
        difflines "$ldwls" "." "$ldwls" "$ldwls"
}

test_worker_count_start_index()
{
    local entry="test_entry"
    local entry2="test_entry2"

    mkdir $entry
    echo "blob" > $entry2

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" \
        --nb-workers 1 --index 0

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" \
        --nb-workers 2 --index 0

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" \
        --nb-workers 2 --index 2

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" \
        --nb-workers 1 --index 4

    rbh_fsevents --enrich rbh:lustre:"$LUSTRE_DIR" \
        src:lustre:"$LUSTRE_MDT" "rbh:$db:$testdb" \
        --nb-workers 4 --index 2

    rbh_log rbh:$db:$testdb --fsevents 5 | grep "parallel" |
        cut -d':' -f2- | sed 's/^[ \t]*//' |
        difflines "4" "1" "2" "2" "1"

    rbh_log rbh:$db:$testdb --fsevents 5 | grep "Starting index" |
        cut -d':' -f2- | sed 's/^[ \t]*//' |
        difflines "2" "4" "2" "0" "0"
}

################################################################################
#                                     MAIN                                     #
################################################################################

declare -a tests=(test_invalid test_fsevents_1 test_fsevents_N test_more_than_N
                  test_timestamps test_command_line test_source_and_enrichment
                  test_worker_count_start_index)

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
