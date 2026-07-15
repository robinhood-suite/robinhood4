/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_LOG_H
#define ROBINHOOD_LOG_H

/**
 * Metadata storage structure
 */
struct rbh_metadata {
    ssize_t converted_entries;
    ssize_t skipped_entries;
};

/**
 * Determines the type of logs to fetch.
 */
enum rbh_log_type {
    RBH_ALL_LOG,
    RBH_FSEVENTS_LOG,
    RBH_SYNC_LOG,
};

static inline const char *
rbh_log_type2str(enum rbh_log_type type)
{
    switch (type) {
    case RBH_ALL_LOG:
        return "all";
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

#endif
