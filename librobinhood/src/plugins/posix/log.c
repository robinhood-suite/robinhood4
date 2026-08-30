/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "robinhood/backends/posix.h"

#include "posix_internals.h"

int
rbh_posix_print_logs(void *plugin_md, char *buffer, size_t buffer_size)
{
    struct rbh_metadata_posix *posix_md = plugin_md;

    return snprintf(buffer, buffer_size,
        "STATS | POSIX:\n"
        "STATS |    regular files seen: %lu\n"
        "STATS |    directories seen: %lu\n"
        "STATS |    symbolic links seen: %lu\n"
        "STATS |    others seen: %lu\n",
        posix_md->file_count,
        posix_md->dir_count,
        posix_md->symlink_count,
        posix_md->other_count
    );
}
