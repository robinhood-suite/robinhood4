/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "stats.h"

enum fsevents_stats_value {
    CHANGELOG_READ,
    DEDUPLICATION_RATIO,
    ENRICH_MOUNTPOINT,
    ENRICH_SKIP_COUNT,
    SOURCE_MD,
    SOURCE_READ,
    START_INDEX,
    TIME_READ_DEDUP,
    TIME_ENRICH_UPDATE,
    WORKER_COUNT,
};

static int
key2fsevents_stats_value(const char *key)
{
    switch (key[0]) {
    case 'c':
        if (!strcmp(&key[1], "hangelog_read"))
            return CHANGELOG_READ;

        break;
    case 'd':
        if (!strcmp(&key[1], "eduplication_ratio"))
            return DEDUPLICATION_RATIO;

        break;
    case 'e':
        if (strncmp(&key[1], "nrich_", 6))
            break;

        if (key[7] == 'm' && !strcmp(&key[8], "ountpoint"))
            return ENRICH_MOUNTPOINT;

        if (key[7] == 's' && !strcmp(&key[8], "kip_count"))
            return ENRICH_SKIP_COUNT;

        break;
    case 's':
        if (key[1] == 'o') {
            if (!strcmp(&key[2], "urce_read"))
                return SOURCE_READ;

            if (!strcmp(&key[2], "urce_md"))
                return SOURCE_MD;

            break;
        }

        if (key[1] == 't' && !strcmp(&key[2], "art_index"))
            return START_INDEX;

        break;
    case 't':
        if (strncmp(&key[1], "ime_", 4))
            break;

        if (key[5] == 'r' && !strcmp(&key[6], "ead_dedup"))
            return TIME_READ_DEDUP;

        if (key[5] == 'e' && !strcmp(&key[6], "nrich_update"))
            return TIME_ENRICH_UPDATE;

        break;
    case 'w':
        if (!strcmp(&key[1], "orker_count"))
            return WORKER_COUNT;

        break;
    }

    error(EXIT_FAILURE, EINVAL, "Unknown key found in fsevents stats: '%s'", key);
    __builtin_unreachable();
}

static const struct formatted_stats_value fsevents_stats_value[] = {
    [CHANGELOG_READ] =      { .header = "Amount of changelog read",
                              .oneline_header = "Changelog read",
                              .print_stats_value = print_value,
                              .oneline = true },
    [DEDUPLICATION_RATIO] = { .header = "Ratio of events deduplicated",
                              .print_stats_value = print_value },
    [ENRICH_MOUNTPOINT] =   { .header = "Enrichment mountpoint",
                              .print_stats_value = print_value },
    [ENRICH_SKIP_COUNT] =   { .header = "Number of skipped entries in enrichment",
                              .print_stats_value = print_value },
    [SOURCE_MD] =           { .header = "Source-specific statistics",
                              .print_stats_value = print_value },
    [SOURCE_READ] =         { .header = "Source of the events",
                              .print_stats_value = print_value },
    [START_INDEX] =         { .header = "Starting index for reading changelogs",
                              .print_stats_value = print_value },
    [TIME_READ_DEDUP] =     { .header = "Time spent reading/deduplicating events",
                              .oneline_header = "Read/dedup time",
                              .print_stats_value = print_timespec,
                              .oneline = true },
    [TIME_ENRICH_UPDATE] =  { .header = "Time spent enriching/updating mirror (on average between all workers)",
                              .oneline_header = "Enrich/update time",
                              .print_stats_value = print_timespec,
                              .oneline = true },
    [WORKER_COUNT] =        { .header = "Number of parallel workers used",
                              .print_stats_value = print_value },
};

void
print_fsevents_stats(const struct rbh_value_map *stats,
                     enum output_format output_format)
{
    print_stats_wrapper(stats, output_format, fsevents_stats_value,
                        &key2fsevents_stats_value);
}
