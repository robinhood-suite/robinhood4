/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

void *
sqlite_cursor_alloc(struct sqlite_cursor *cursor, size_t count)
{
    return rbh_sstack_alloc(cursor->sstack, count);
}

char *
sqlite_cursor_strdup(struct sqlite_cursor *cursor, const char *str)
{
    return rbh_sstack_strdup(cursor->sstack, str);
}

void
sqlite_cursor_free(struct sqlite_cursor *cursor)
{
    rbh_sstack_pop_all(cursor->sstack);
}
