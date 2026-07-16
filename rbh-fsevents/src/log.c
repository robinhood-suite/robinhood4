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
    struct rbh_value_map *value_map;
    struct rbh_value_pair *pairs;
    struct rbh_value *values;
    int count = 7;

    if (metadata_sstack == NULL)
        metadata_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                         (sizeof(struct rbh_value_map *)));

    value_map = RBH_SSTACK_PUSH(metadata_sstack, NULL, sizeof(*value_map));
    values = RBH_SSTACK_PUSH(metadata_sstack, NULL, count * sizeof(*values));
    pairs = RBH_SSTACK_PUSH(metadata_sstack, NULL, count * sizeof(*pairs));

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
