/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <unistd.h>

#include <robinhood/backend.h>
#include <robinhood/fsentry.h>

int
rbh_lustre_delete_entry(struct rbh_fsentry *fsentry)
{
    return unlink(fsentry_relative_path(fsentry));
}
