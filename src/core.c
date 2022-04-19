/* This file is part of rbh-find
 * Copyright (C) 2022 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "rbh-find/core.h"

void
ctx_finish(struct find_context *ctx)
{
    for (size_t i = 0; i < ctx->backend_count; i++)
        rbh_backend_destroy(ctx->backends[i]);
    free(ctx->backends);
}
