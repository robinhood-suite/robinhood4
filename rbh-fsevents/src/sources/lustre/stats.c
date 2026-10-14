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
