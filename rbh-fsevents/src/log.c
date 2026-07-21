/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <stdlib.h>

#include <robinhood/sstack.h>
#include <robinhood/utils.h>

#include "log.h"

#define MIN_VALUES_SSTACK_ALLOC (1 << 6)
static __thread struct rbh_sstack *metadata_sstack;

static void __attribute__ ((destructor))
destroy_metadata_sstack(void)
{
    if (metadata_sstack)
        rbh_sstack_destroy(metadata_sstack);
}

static struct rbh_value_map *
fsevents_metadata_value_map(struct rbh_metadata *metadata)
{
    struct rbh_value_pair *pairs_timespec;
    struct rbh_value_map *timespec_map;
    struct rbh_value *values_timespec;
    struct rbh_value_map *value_map;
    struct rbh_value_pair *pairs;
    struct rbh_value *values;
    int count_timespec = 4;
    int count = 12;

    if (metadata_sstack == NULL)
        metadata_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                         (sizeof(struct rbh_value_map *)));

    value_map = RBH_SSTACK_PUSH(metadata_sstack, NULL, sizeof(*value_map));
    values = RBH_SSTACK_PUSH(metadata_sstack, NULL,
                             (count + count_timespec) * sizeof(*values));
    pairs = RBH_SSTACK_PUSH(metadata_sstack, NULL, count * sizeof(*pairs));

    timespec_map = RBH_SSTACK_PUSH(metadata_sstack, NULL,
                                   (count_timespec / 2) *
                                        sizeof(*timespec_map));
    values_timespec = RBH_SSTACK_PUSH(metadata_sstack, NULL,
                                      count_timespec * sizeof(*values));
    pairs_timespec = RBH_SSTACK_PUSH(metadata_sstack, NULL,
                                     4 * sizeof(*pairs_timespec));

    rbh_set_common_metadata_pairs(&metadata->common_md, values, pairs);

    pairs[4].key = "source_read";
    values[4].type = RBH_VT_STRING;
    values[4].string = metadata->fsevents_md.source_read;
    pairs[4].value = &values[4];

    pairs[5].key = "enrich_mountpoint";
    values[5].type = RBH_VT_STRING;
    values[5].string = metadata->fsevents_md.enrich_mountpoint;
    pairs[5].value = &values[5];

    pairs[6].key = "worker_count";
    values[6].type = RBH_VT_UINT64;
    values[6].uint64 = metadata->fsevents_md.worker_count;
    pairs[6].value = &values[6];

    pairs_timespec[0].key = "tv_sec";
    values_timespec[0].type = RBH_VT_UINT64;
    values_timespec[0].uint64 =
        metadata->fsevents_md.time_spent_read_and_dedup.tv_sec;
    pairs_timespec[0].value = &values_timespec[0];

    pairs_timespec[1].key = "tv_nsec";
    values_timespec[1].type = RBH_VT_UINT64;
    values_timespec[1].uint64 =
        metadata->fsevents_md.time_spent_read_and_dedup.tv_nsec;
    pairs_timespec[1].value = &values_timespec[1];

    timespec_map[0].pairs = &pairs_timespec[0];
    timespec_map[0].count = 2;

    pairs[7].key = "time_read_dedup";
    values[7].type = RBH_VT_MAP;
    values[7].map = timespec_map[0];
    pairs[7].value = &values[7];

    pairs_timespec[2].key = "tv_sec";
    values_timespec[2].type = RBH_VT_UINT64;
    values_timespec[2].uint64 =
        metadata->fsevents_md.time_spent_enrich_and_update.tv_sec;
    pairs_timespec[2].value = &values_timespec[2];

    pairs_timespec[3].key = "tv_nsec";
    values_timespec[3].type = RBH_VT_UINT64;
    values_timespec[3].uint64 =
        metadata->fsevents_md.time_spent_enrich_and_update.tv_nsec;
    pairs_timespec[3].value = &values_timespec[3];

    timespec_map[1].pairs = &pairs_timespec[2];
    timespec_map[1].count = 2;

    pairs[8].key = "time_enrich_update";
    values[8].type = RBH_VT_MAP;
    values[8].map = timespec_map[1];
    pairs[8].value = &values[8];

    pairs[9].key = "changelog_read";
    values[9].type = RBH_VT_UINT64;
    values[9].uint64 = metadata->fsevents_md.changelog_read;
    pairs[9].value = &values[9];

    pairs[10].key = "start_index";
    values[10].type = RBH_VT_INT64;
    values[10].int64 = metadata->fsevents_md.start_index;
    pairs[10].value = &values[10];

    pairs[11].key = "enrich_skip_count";
    values[11].type = RBH_VT_UINT64;
    values[11].uint64 = metadata->fsevents_md.enrich_skip_count;
    pairs[11].value = &values[11];

    value_map->pairs = pairs;
    value_map->count = count;

    return value_map;
}

void
insert_fsevents_log(struct sink *sink, struct rbh_metadata *metadata)
{
    if (!sink_insert_log(sink, fsevents_metadata_value_map(metadata)))
        return;

    switch (errno) {
    case 0:
        fprintf(stderr, "failed to insert log\n");
        break;
    case ENOTSUP:
        break;
    case RBH_BACKEND_ERROR:
        fprintf(stderr, "failed to insert log: %s\n", rbh_backend_error);
        break;
    default:
        fprintf(stderr, "failed to insert log: %s\n", strerror(errno));
        break;
    }
}
