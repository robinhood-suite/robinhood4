/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

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
find_metadata_value_map(struct rbh_metadata *metadata)
{
    struct rbh_value_map *value_map;
    struct rbh_value_pair *pairs;
    struct rbh_value *values;
    int count = 4;

    if (metadata_sstack == NULL)
        metadata_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                         (sizeof(struct rbh_value_map *)));

    value_map = RBH_SSTACK_PUSH(metadata_sstack, NULL, sizeof(*value_map));
    values = RBH_SSTACK_PUSH(metadata_sstack, NULL, count * sizeof(*values));
    pairs = RBH_SSTACK_PUSH(metadata_sstack, NULL, count * sizeof(*pairs));

    rbh_set_common_metadata_pairs(&metadata->common_md, values, pairs);

    value_map->pairs = pairs;
    value_map->count = count;

    return value_map;
}

void
insert_find_log(struct find_context *ctx, struct rbh_metadata *metadata)
{
    struct rbh_value_map *metadata_map = find_metadata_value_map(metadata);

    for (size_t i = 0; i < ctx->backend_count; ++i) {
        errno = 0;

        if (!rbh_backend_insert_log(ctx->backends[i], "find", metadata_map))
            continue;

        switch (errno) {
        case 0:
            fprintf(stderr, "failed to insert find log\n");
            break;
        case RBH_BACKEND_ERROR:
            fprintf(stderr, "failed to insert find log: %s\n",
                    rbh_backend_error);
            break;
        default:
            fprintf(stderr, "failed to insert find log: %s\n", strerror(errno));
            break;
        }
    }
}

