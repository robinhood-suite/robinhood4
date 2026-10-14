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
