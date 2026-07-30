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
    int count = 13;

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
                                     count_timespec * sizeof(*pairs_timespec));

    rbh_set_common_metadata_pairs(&metadata->common_md, values, pairs);

    count = 4;

    pairs[count].key = "source_read";
    values[count].type = RBH_VT_STRING;
    values[count].string = metadata->fsevents_md.source_read;
    pairs[count].value = &values[count];
    count++;

    if (metadata->fsevents_md.enrich_mountpoint) {
        pairs[count].key = "enrich_mountpoint";
        values[count].type = RBH_VT_STRING;
        values[count].string = metadata->fsevents_md.enrich_mountpoint;
        pairs[count].value = &values[count];
        count++;
    }

    pairs[count].key = "worker_count";
    values[count].type = RBH_VT_UINT64;
    values[count].uint64 = metadata->fsevents_md.worker_count;
    pairs[count].value = &values[count];
    count++;

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

    pairs[count].key = "time_read_dedup";
    values[count].type = RBH_VT_MAP;
    values[count].map = timespec_map[0];
    pairs[count].value = &values[count];
    count++;

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

    pairs[count].key = "time_enrich_update";
    values[count].type = RBH_VT_MAP;
    values[count].map = timespec_map[1];
    pairs[count].value = &values[count];
    count++;

    pairs[count].key = "changelog_read";
    values[count].type = RBH_VT_UINT64;
    values[count].uint64 = metadata->fsevents_md.changelog_read;
    pairs[count].value = &values[count];
    count++;

    pairs[count].key = "start_index";
    values[count].type = RBH_VT_INT64;
    values[count].int64 = metadata->fsevents_md.start_index;
    pairs[count].value = &values[count];
    count++;

    if (metadata->fsevents_md.enrich_mountpoint) {
        pairs[count].key = "enrich_skip_count";
        values[count].type = RBH_VT_UINT64;
        values[count].uint64 = metadata->fsevents_md.enrich_skip_count;
        pairs[count].value = &values[count];
        count++;
    }

    pairs[count].key = "deduplication_ratio";
    values[count].type = RBH_VT_DOUBLE;

    if (metadata->fsevents_md.event_amount)
        values[count].float64 =
            (double) (metadata->fsevents_md.deduplicated_event_amount * 100) /
                (double) metadata->fsevents_md.event_amount;
    else
        values[count].float64 = 0.0;

    pairs[count].value = &values[count];
    count++;

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
