/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "lustre.h"

int
print_lustre_source_md(struct rbh_fsevents_metadata *fsevents_md,
                       char *message, size_t *message_avail_size)
{
    struct lustre_source_md *lustre_md = fsevents_md->source_md;
    int counter = 0;
    int rc;

    rc = snprintf(&message[counter], *message_avail_size,
                  "STATS |      Lustre source stats:\n");
    if (rc < 0)
        return rc;

    counter += rc;
    *message_avail_size -= rc;

    for (int i = 0; i < CL_LAST; i++) {
        rc = snprintf(&message[counter], *message_avail_size,
                      "STATS |          %s: %lu\n",
                      changelog_type2str(i),
                      lustre_md->changelog_count[i]);
        if (rc < 0)
            return rc;

        counter += rc;
        *message_avail_size -= rc;
    }

    return counter;
}

#define MIN_VALUES_SSTACK_ALLOC (1 << 10)
static __thread struct rbh_sstack *metadata_sstack;

static void __attribute__ ((destructor))
destroy_metadata_sstack(void)
{
    if (metadata_sstack)
        rbh_sstack_destroy(metadata_sstack);
}

struct rbh_value_map *
convert_lustre_source_md(struct rbh_fsevents_metadata *fsevents_md)
{
    struct lustre_source_md *lustre_md = fsevents_md->source_md;
    struct rbh_value_map *value_map;
    struct rbh_value_pair *pairs;
    struct rbh_value *values;
    int count = CL_LAST;

    if (metadata_sstack == NULL)
        metadata_sstack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                                         (sizeof(struct rbh_value_map *)));

    value_map = RBH_SSTACK_PUSH(metadata_sstack, NULL, sizeof(*value_map));
    values = RBH_SSTACK_PUSH(metadata_sstack, NULL, count * sizeof(*values));
    pairs = RBH_SSTACK_PUSH(metadata_sstack, NULL, count * sizeof(*pairs));

    for (count = 0; count < CL_LAST; ++count) {
        pairs[count].key = changelog_type2str(count);
        values[count].type = RBH_VT_UINT64;
        values[count].uint64 = lustre_md->changelog_count[count];
        pairs[count].value = &values[count];
    }

    value_map->pairs = pairs;
    value_map->count = count;

    return value_map;
}
