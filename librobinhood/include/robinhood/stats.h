/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_STATS_H
#define ROBINHOOD_STATS_H

#include <stdio.h>
#include <time.h>

#include "robinhood/value.h"

struct rbh_common_metadata {
    time_t start_time;
    time_t end_time;
    char *command_line;
};

struct rbh_sync_metadata {
    char *source_mountpoint;
    ssize_t converted_entries;
    ssize_t skipped_entries;
};

struct rbh_fsevents_metadata {
    const char *source_read;
    const char *enrich_mountpoint;
    size_t worker_count;
    struct timespec time_spent_read_and_dedup;
    _Atomic(struct timespec) time_spent_enrich_and_update;
    size_t changelog_read;
    int64_t start_index;
    size_t enrich_skip_count;
    size_t deduplicated_event_amount;
    size_t event_amount;
};

struct rbh_find_metadata {
    size_t entry_count;
    int64_t exec_success_count;
};

struct rbh_gc_metadata {
    int64_t sync_time;
    char *check_command;
    size_t deleted_entry_count;
    size_t total_entry_count;
};

/**
 * Metadata storage structure
 */
struct rbh_metadata {
    struct rbh_common_metadata common_md;
    void *plugin_md;
    union {
        struct rbh_sync_metadata sync_md;
        struct rbh_fsevents_metadata fsevents_md;
        struct rbh_find_metadata find_md;
        struct rbh_gc_metadata gc_md;
    };
    time_t last_shown_time;
    FILE *stats_file;
    int64_t stats_interval;
};

/**
 * Determines the type of stats to fetch.
 */
enum rbh_stats_type {
    RBH_ALL_STATS      = 0x00,
    RBH_FIND_STATS     = 0x01,
    RBH_FSEVENTS_STATS = 0x02,
    RBH_GC_STATS       = 0x04,
    RBH_REPORT_STATS   = 0x08,
    RBH_SYNC_STATS     = 0x10,

    RBH_STATS_TYPE_FIRST = RBH_FIND_STATS,
    RBH_STATS_TYPE_LAST = RBH_SYNC_STATS,
    RBH_STATS_TYPE_COUNT = 5,
};

static inline const char *
rbh_stats_type2str(enum rbh_stats_type type)
{
    switch (type) {
    case RBH_ALL_STATS:
        return "all";
    case RBH_FIND_STATS:
        return "find";
    case RBH_FSEVENTS_STATS:
        return "fsevents";
    case RBH_GC_STATS:
        return "gc";
    case RBH_REPORT_STATS:
        return "report";
    case RBH_SYNC_STATS:
        return "sync";
    default:
        return "invalid";
    };

   __builtin_unreachable();
}

static inline enum rbh_stats_type
str2rbh_stats_type(const char *str)
{
    if (!strcmp(str, "find"))
        return RBH_FIND_STATS;
    if (!strcmp(str, "fsevents"))
        return RBH_FSEVENTS_STATS;
    if (!strcmp(str, "gc"))
        return RBH_GC_STATS;
    if (!strcmp(str, "report"))
        return RBH_REPORT_STATS;
    if (!strcmp(str, "sync"))
        return RBH_SYNC_STATS;

    return RBH_ALL_STATS;
}

/**
 * Determines which stats should be fetched from the backend, the amount and the
 * sorting order.
 */
struct rbh_stats_options {
    size_t type;
    size_t count;
    bool ascending;
    uint64_t end_timestamp;
    uint64_t start_timestamp;
};

static inline void
rbh_set_common_metadata_pairs(struct rbh_common_metadata *md,
                              struct rbh_value *values,
                              struct rbh_value_pair *pairs)
{
    pairs[0].key = "start_time";
    values[0].type = RBH_VT_INT64;
    values[0].int64 = (int64_t) md->start_time;
    pairs[0].value = &values[0];

    pairs[1].key = "duration";
    values[1].type = RBH_VT_INT64;
    values[1].int64 = (int64_t) difftime(md->end_time, md->start_time);
    pairs[1].value = &values[1];

    pairs[2].key = "end_time";
    values[2].type = RBH_VT_INT64;
    values[2].int64 = (int64_t) md->end_time;
    pairs[2].value = &values[2];

    pairs[3].key = "command_line";
    values[3].type = RBH_VT_STRING;
    values[3].string = md->command_line;
    pairs[3].value = &values[3];
}

static inline bool
rbh_should_print_stats(struct rbh_metadata *metadata)
{
    return metadata->stats_interval > 0 &&
           time(NULL) - metadata->last_shown_time >= metadata->stats_interval;
}

void
rbh_print_stats(struct rbh_metadata *metadata, enum rbh_stats_type command_type,
              const char *plugin_name);

void
rbh_timespec_atomic_accumulate(struct rbh_fsevents_metadata *fsevents_md,
                               struct timespec to_add);

#endif
