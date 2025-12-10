/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "utils.h"

#include <robinhood.h>

static struct rbh_mut_iterator *
get_entry_children(struct rbh_backend *backend, struct rbh_fsentry *entry)
{
    const struct rbh_filter_field *field;
    struct rbh_filter *filter;

    field = str2filter_field("parent-id");
    filter = rbh_filter_compare_binary_new(RBH_FOP_EQUAL, field,
                                           entry->id.data, entry->id.size);
    if (filter == NULL)
        error(EXIT_FAILURE, errno, "failed to create filter");

    return get_entries(backend, filter);
}

struct rbh_node_fsevent {
    struct rbh_fsevent *fsevent;
    struct rbh_list_node link;
};

struct list_iterator {
    struct rbh_iterator iterator;
    struct rbh_iterator *list;
};

static const void *
fsevent_iter_next(void *iterator)
{
    struct list_iterator *iter = iterator;
    const struct rbh_node_fsevent *node;

    node = rbh_iter_next(iter->list);
    if (node == NULL)
        return NULL;

    return node->fsevent;
}

static void
list_iter_destroy(void *iterator)
{
    struct list_iterator *iter = iterator;

    rbh_iter_destroy(iter->list);
    free(iter);
}

static const struct rbh_iterator_operations FSEVENT_ITER_OPS = {
    .next = fsevent_iter_next,
    .destroy = list_iter_destroy,
};

static const struct rbh_iterator FSEVENT_ITERATOR = {
    .ops = &FSEVENT_ITER_OPS,
};

__attribute__((unused))
static struct rbh_iterator *
new_iter_list(struct rbh_iterator *list)
{
    struct list_iterator *iter;

    iter = xmalloc(sizeof(*iter));

    iter->iterator = FSEVENT_ITERATOR;
    iter->list = list;
    return &iter->iterator;
}

bool
remove_children_path(struct rbh_backend *backend, struct rbh_fsentry *entry)
{
    struct rbh_mut_iterator *children;
    struct rbh_iterator *update_iter;
    struct rbh_fsevent *fsevent;
    bool need_update = false;
    int rc;

    children = get_entry_children(backend, entry);

    while (true) {
        struct rbh_fsentry *child = rbh_mut_iter_next(children);

        if (child == NULL) {
            if (errno == ENODATA)
                break;

            if (errno == RBH_BACKEND_ERROR)
                error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
            else
                error(EXIT_FAILURE, errno, "failed to retrieve child");
        }

        /* TODO: store all the fsevent in a list and call rbh_backend_update
         * one time
         */
        fsevent = generate_fsevent_ns_xattrs(child, NULL);
        update_iter = rbh_iter_array(fsevent,sizeof(*fsevent), 1, NULL);

        rc = rbh_backend_update(backend, update_iter);
        if (rc == -1)
            error(EXIT_FAILURE, errno, "failed to update '%s'", child->name);

        if (!need_update)
            need_update = true;
        rbh_iter_destroy(update_iter);
        free(fsevent);
        free(child);
    }

    return need_update;
}

