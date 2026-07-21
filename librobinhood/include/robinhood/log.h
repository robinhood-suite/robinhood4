/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_LOG_H
#define ROBINHOOD_LOG_H

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
    struct timespec time_spent_enrich_and_update;
    size_t changelog_read;
    int64_t start_index;
    size_t enrich_skip_count;
};

struct rbh_find_metadata {
    size_t entry_count;
    int64_t exec_success_count;
};

/**
 * Metadata storage structure
 */
struct rbh_metadata {
    struct rbh_common_metadata common_md;
    union {
        struct rbh_sync_metadata sync_md;
        struct rbh_fsevents_metadata fsevents_md;
        struct rbh_find_metadata find_md;
    };
};

/**
 * Determines the type of logs to fetch.
 */
enum rbh_log_type {
    RBH_ALL_LOG,
    RBH_FIND_LOG,
    RBH_FSEVENTS_LOG,
    RBH_SYNC_LOG,
};

static inline const char *
rbh_log_type2str(enum rbh_log_type type)
{
    switch (type) {
    case RBH_ALL_LOG:
        return "all";
    case RBH_FIND_LOG:
        return "find";
    case RBH_FSEVENTS_LOG:
        return "fsevents";
    case RBH_SYNC_LOG:
        return "sync";
    default:
        return "invalid";
    };

   __builtin_unreachable();
}

static inline enum rbh_log_type
str2rbh_log_type(const char *str)
{
    if (!strcmp(str, "find"))
        return RBH_FIND_LOG;
    if (!strcmp(str, "fsevents"))
        return RBH_FSEVENTS_LOG;
    if (!strcmp(str, "sync"))
        return RBH_SYNC_LOG;

    return RBH_ALL_LOG;
}

/**
 * Determines which logs should be fetched from the backend, the amount and the
 * sorting order.
 */
struct rbh_log_options {
    enum rbh_log_type type;
    size_t count;
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

#endif
