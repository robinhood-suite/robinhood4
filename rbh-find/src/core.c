/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdbool.h>
#include <string.h>
#include <sysexits.h>

#include <robinhood/filters/core.h>

#include "actions.h"
#include "filters.h"
#include "core.h"

void
ctx_finish(struct find_context *ctx)
{
    const char *name;

    for (size_t i = 0; i < ctx->backend_count; i++) {
        name = ctx->backends[i]->name;
        rbh_backend_destroy(ctx->backends[i]);
        rbh_backend_plugin_destroy(name);
    }
    free(ctx->backends);
    filters_ctx_finish(&ctx->f_ctx);
}

/**
 * Filter through every fsentries in a specific backend, executing the
 * requested action on each of them
 *
 * @param ctx            find's context for this execution
 * @param backend_index  index of the backend to go through
 * @param action         the type of action to execute
 * @param filter         the filter to apply to each fsentry
 * @param sorts          how the list of retrieved fsentries is sorted
 * @param sorts_count    how many fsentries to sort
 *
 * @return               the number of fsentries checked
 */
static size_t
_find(struct find_context *ctx, int backend_index, enum action action,
      const struct rbh_filter *filter, const struct rbh_filter_sort *sorts,
      size_t sorts_count)
{
    const struct rbh_filter_options OPTIONS = {
        .sort = {
            .items = sorts,
            .count = sorts_count
        },
    };
    const struct rbh_filter_output OUTPUT = {
        .type = RBH_FOT_PROJECTION,
        .projection = {
            .fsentry_mask = RBH_FP_ALL,
            .statx_mask = RBH_STATX_ALL,
        },
    };
    struct rbh_mut_iterator *fsentries;
    size_t count = 0;

    /**
     * Some filters need to be completed with value from the backend before
     * doing the find.
     */
    if (ctx->f_ctx.need_prefetch &&
        complete_rbh_filter(filter, ctx->backends[backend_index], &OPTIONS,
                            &OUTPUT))
        return count;

    fsentries = rbh_backend_filter(ctx->backends[backend_index], filter,
                                   &OPTIONS, &OUTPUT, NULL);
    if (fsentries == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "filter_fsentries");

    do {
        struct rbh_fsentry *fsentry;

        do {
            errno = 0;
            fsentry = rbh_mut_iter_next(fsentries);
        } while (fsentry == NULL && errno == EAGAIN);

        if (fsentry == NULL)
            break;

        count += find_exec_action(ctx, backend_index, action, fsentry);
        free(fsentry);
    } while (true);

    if (errno != ENODATA)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_mut_iter_next");

    rbh_mut_iter_destroy(fsentries);

    return count;
}

void
find(struct find_context *ctx, enum action action, int *arg_idx,
     const struct rbh_filter *filter, const struct rbh_filter_sort *sorts,
     size_t sorts_count)
{
    int i = *arg_idx;
    size_t count = 0;

    ctx->action_done = true;

    i += find_pre_action(ctx, i, action);

    for (size_t i = 0; i < ctx->backend_count; i++)
        count += _find(ctx, i, action, filter, sorts, sorts_count);

    find_post_action(ctx, i, action, count);

    *arg_idx = i;
}
