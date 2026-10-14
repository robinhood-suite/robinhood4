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
    int rc;

    rc = snprintf(message, *message_avail_size,
        "STATS |      Lustre source stats:\n"
        "STATS |          CREAT: %lu\n",
        lustre_md->changelog_count[CL_CREATE]
    );
    if (rc < 0)
        return rc;

    *message_avail_size -= rc;
    return rc;
}
