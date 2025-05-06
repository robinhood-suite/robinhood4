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

static int complete_rbh_filter(const struct rbh_filter *filter,
                               struct rbh_backend *backend,
                               const struct rbh_filter_options *options,
                               const struct rbh_filter_output *output);

static int
complete_logical_filter(const struct rbh_filter *filter,
                        struct rbh_backend *backend,
                        const struct rbh_filter_options *options,
                        const struct rbh_filter_output *output)
{
    for (uint32_t i = 0; i < filter->logical.count; i++) {
        if (complete_rbh_filter(filter->logical.filters[i], backend, options,
                                output))
            return -1;
    }

    return 0;
}

static void
update_statx_rbh_value(struct rbh_filter *filter,
                       const struct rbh_filter_field *field,
                       const struct rbh_statx *statx)
{
    switch (field->statx) {
    case RBH_STATX_MTIME_SEC:
        filter->compare.value.uint64 = statx->stx_mtime.tv_sec;
        return;
    default:
        return;
    }
}

static void
update_rbh_value(struct rbh_filter *filter,
                 const struct rbh_filter_field *field,
                 struct rbh_fsentry *fsentry)
{
    switch (field->fsentry) {
    case RBH_FP_STATX:
        return update_statx_rbh_value(filter, field, fsentry->statx);
    default:
        return;
    }
}

static int
complete_get_filter(const struct rbh_filter *filter,
                    struct rbh_backend *backend,
                    const struct rbh_filter_options *options,
                    const struct rbh_filter_output *output)
{
    struct rbh_mut_iterator *fsentries;
    struct rbh_fsentry *fsentry;

    fsentries = rbh_backend_filter(backend, filter->get.fsentry_to_get, options,
                                   output);
    if (fsentries == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "filter_fsentries");

    fsentry = rbh_mut_iter_next(fsentries);
    if (fsentry == NULL) {
        rbh_mut_iter_destroy(fsentries);
        return -1;
    }

    update_rbh_value(filter->get.filter, &filter->get.field, fsentry);

    free(fsentry);
    rbh_mut_iter_destroy(fsentries);

    return 0;
}

static int
complete_rbh_filter(const struct rbh_filter *filter,
                    struct rbh_backend *backend,
                    const struct rbh_filter_options *options,
                    const struct rbh_filter_output *output)
{
    if (rbh_is_logical_operator(filter->op))
        return complete_logical_filter(filter, backend, options, output);

    if (rbh_is_get_operator(filter->op))
        return complete_get_filter(filter, backend, options, output);

    return 0;
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
                                   &OPTIONS, &OUTPUT);
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
