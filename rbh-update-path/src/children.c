/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "utils.h"

#include <robinhood.h>

#ifndef RBH_ITER_CHUNK_SIZE
# define RBH_ITER_CHUNK_SIZE (1 << 12)
#endif

#define MIN_VALUES_SSTACK_ALLOC (1 << 6)

static __thread struct rbh_sstack *stack =  NULL;

__attribute__((destructor))
void
free_stack(void)
{
    if (stack)
        rbh_sstack_destroy(stack);
}

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

struct rbh_node_data {
    void *data;
    struct rbh_list_node link;
};

static void
add_data_child(struct rbh_list_node *list, void *data)
{
    struct rbh_node_data *node;

    if (stack == NULL)
        stack = rbh_sstack_new(MIN_VALUES_SSTACK_ALLOC *
                               sizeof(struct rbh_node_data *));

    node = RBH_SSTACK_PUSH(stack, NULL, sizeof(*node));
    node->data = data;
    rbh_list_add_tail(list, &node->link);
}

static void
free_data_list(struct rbh_list_node *list)
{
    struct rbh_node_data *node, *tmp;

    rbh_list_foreach_safe(list, node, tmp, link)
        free(node->data);

    free(list);
}

struct list_iterator {
    struct rbh_iterator iterator;
    struct rbh_iterator *list;
};

static const void *
list_iter_next(void *iterator)
{
    struct list_iterator *iter = iterator;
    const struct rbh_node_data *node;

    node = rbh_iter_next(iter->list);
    if (node == NULL)
        return NULL;

    return node->data;
}

static void
list_iter_destroy(void *iterator)
{
    struct list_iterator *iter = iterator;

    rbh_iter_destroy(iter->list);
    free(iter);
}

static const struct rbh_iterator_operations FSEVENT_ITER_OPS = {
    .next = list_iter_next,
    .destroy = list_iter_destroy,
};

static const struct rbh_iterator FSEVENT_ITERATOR = {
    .ops = &FSEVENT_ITER_OPS,
};

static struct rbh_iterator *
new_iter_list(struct rbh_iterator *list)
{
    struct list_iterator *iter;

    iter = xmalloc(sizeof(*iter));

    iter->iterator = FSEVENT_ITERATOR;
    iter->list = list;
    return &iter->iterator;
}

static struct rbh_mut_iterator *
_rbh_mut_iter_list(struct rbh_list_node *list)
{
    struct rbh_iterator *list_iter;

    list_iter = rbh_iter_list(list, offsetof(struct rbh_node_data, link),
                              free_data_list);

    return (struct rbh_mut_iterator *) new_iter_list(list_iter);
}

static struct rbh_iterator *
_rbh_iter_list(struct rbh_list_node *list)
{
    struct rbh_iterator *list_iter;

    list_iter = rbh_iter_list(list, offsetof(struct rbh_node_data, link),
                              free_data_list);

    return new_iter_list(list_iter);
}

static struct rbh_list_node *
build_fsevents_remove_path(struct rbh_mut_iterator *children,
                           struct rbh_list_node *batches)
{
    struct rbh_list_node *fsentries;
    struct rbh_list_node *fsevents;

    fsevents = xmalloc(sizeof(*fsevents));
    rbh_list_init(fsevents);

    fsentries = xmalloc(sizeof(*fsentries));
    rbh_list_init(fsentries);

    while (true) {
        struct rbh_fsevent *fsevent;
        struct rbh_fsentry *child = rbh_mut_iter_next(children);

        if (child == NULL) {
            if (errno == ENODATA)
                break;

            if (errno == RBH_BACKEND_ERROR)
                error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
            else
                error(EXIT_FAILURE, errno, "failed to retrieve child");
        }

        add_data_child(fsentries, child);
        fsevent = generate_fsevent_ns_xattrs(child, NULL);
        add_data_child(fsevents, fsevent);
    }

    if (rbh_list_empty(fsevents)) {
        free(fsevents);
        free(fsentries);
        return NULL;
    }

    if (!rbh_list_empty(fsentries)) {
        struct rbh_mut_iterator *iter = _rbh_mut_iter_list(fsentries);
        add_iterator(batches, iter);
    }

    return fsevents;
}

void
remove_children_path(struct rbh_backend *backend, struct rbh_fsentry *entry,
                     struct rbh_list_node *batches)
{
    struct rbh_mut_iterator *children;
    struct rbh_iterator *update_iter;
    struct rbh_mut_iterator *chunks;
    struct rbh_list_node *fsevents;

    children = get_entry_children(backend, entry);

    fsevents = build_fsevents_remove_path(children, batches);

    rbh_mut_iter_destroy(children);

    if (fsevents == NULL)
        return;

    update_iter = _rbh_iter_list(fsevents);

    /* the mongo backend tries to process all the fsevents at once in a
     * single bulk operation, but a bulk operation is limited in size.
     *
     * Splitting `fsevents' into fixed-size sub-iterators solves this.
     */
    chunks = rbh_iter_chunkify(update_iter, RBH_ITER_CHUNK_SIZE);
    if (chunks == NULL)
        error(EXIT_FAILURE, errno, "rbh_mut_iter_chunkify");

    do {
        struct rbh_iterator *chunk = rbh_mut_iter_next(chunks);
        int save_errno;
        ssize_t count;

        if (chunk == NULL) {
            if (errno == ENODATA)
                break;
            error(EXIT_FAILURE, errno, "while chunkifying fsevents");
        }

        count = rbh_backend_update(backend, chunk);
        save_errno = errno;
        rbh_iter_destroy(chunk);
        if (count < 0) {
            errno = save_errno;
            error(EXIT_FAILURE, errno, "rbh_backend_update");
        }
    } while (true);

    rbh_mut_iter_destroy(chunks);
}
