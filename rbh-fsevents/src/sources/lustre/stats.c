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
        "STATS |          MARK: %lu\n"
        "STATS |          CREAT: %lu\n"
        "STATS |          MKDIR: %lu\n"
        "STATS |          HLINK: %lu\n"
        "STATS |          SLINK: %lu\n"
        "STATS |          MKNOD: %lu\n"
        "STATS |          UNLNK: %lu\n"
        "STATS |          RMDIR: %lu\n"
        "STATS |          RENME: %lu\n"
        "STATS |          RNMTO: %lu\n"
        "STATS |          OPEN: %lu\n"
        "STATS |          CLOSE: %lu\n"
        "STATS |          LYOUT: %lu\n"
        "STATS |          TRUNC: %lu\n"
        "STATS |          SATTR: %lu\n"
        "STATS |          XATTR: %lu\n"
        "STATS |          HSM: %lu\n"
        "STATS |          MTIME: %lu\n"
        "STATS |          CTIME: %lu\n"
        "STATS |          ATIME: %lu\n"
        "STATS |          MIGRT: %lu\n"
        "STATS |          FLRW: %lu\n"
        "STATS |          RESYNC: %lu\n"
        "STATS |          GXATR: %lu\n"
        "STATS |          NOPEN: %lu\n",
        lustre_md->changelog_count[CL_MARK],
        lustre_md->changelog_count[CL_CREATE],
        lustre_md->changelog_count[CL_MKDIR],
        lustre_md->changelog_count[CL_HARDLINK],
        lustre_md->changelog_count[CL_SOFTLINK],
        lustre_md->changelog_count[CL_MKNOD],
        lustre_md->changelog_count[CL_UNLINK],
        lustre_md->changelog_count[CL_RMDIR],
        lustre_md->changelog_count[CL_RENAME],
        lustre_md->changelog_count[CL_EXT],
        lustre_md->changelog_count[CL_OPEN],
        lustre_md->changelog_count[CL_CLOSE],
        lustre_md->changelog_count[CL_LAYOUT],
        lustre_md->changelog_count[CL_TRUNC],
        lustre_md->changelog_count[CL_SETATTR],
        lustre_md->changelog_count[CL_SETXATTR],
        lustre_md->changelog_count[CL_HSM],
        lustre_md->changelog_count[CL_MTIME],
        lustre_md->changelog_count[CL_CTIME],
        lustre_md->changelog_count[CL_ATIME],
        lustre_md->changelog_count[CL_MIGRATE],
        lustre_md->changelog_count[CL_FLRW],
        lustre_md->changelog_count[CL_RESYNC],
        lustre_md->changelog_count[CL_GETXATTR],
        lustre_md->changelog_count[CL_DN_OPEN]
    );
    if (rc < 0)
        return rc;

    *message_avail_size -= rc;
    return rc;
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
