/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static bool
scan_inodes(struct ldiskfs_backend *backend)
{
    return true;
}

static bool
scan_dentries(struct ldiskfs_backend *backend)
{
    return true;
}

bool
scan_target(struct ldiskfs_backend *backend)
{
    return scan_inodes(backend) &&
        scan_dentries(backend);
}
