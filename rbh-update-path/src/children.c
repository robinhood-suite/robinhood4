/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "utils.h"

#include <robinhood.h>

#ifndef RBH_ITER_CHUNK_SIZE
# define RBH_ITER_CHUNK_SIZE (1 << 12)
#endif

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
    struct rbh_fsevent fsevent;
    struct rbh_list_node link;
};

static int
add_fsevents_child(struct rbh_list_node *list, struct rbh_fsevent *fsevent,
                   struct rbh_sstack *stack)
{
    struct rbh_node_fsevent *node = xmalloc(sizeof(*node));

    rbh_fsevent_deep_copy(&node->fsevent, fsevent, stack);

    rbh_list_add_tail(list, &node->link);

    return 0;
}

static void
free_fsevents_list(struct rbh_list_node *list)
{
    struct rbh_node_fsevent *elem, *tmp;

    rbh_list_foreach_safe(list, elem, tmp, link) {
        free(elem);
    }

    free(list);
}

static struct rbh_list_node *
build_fsevents_remove_path(struct rbh_mut_iterator *children,
                           struct rbh_sstack *stack)
{
    struct rbh_list_node *fsevents;

    fsevents = xmalloc(sizeof(*fsevents));
    rbh_list_init(fsevents);

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

        fsevent = generate_fsevent_ns_xattrs(child, NULL);
        add_fsevents_child(fsevents, fsevent, stack);
        free(fsevent);
        free(child);
    }

    if (rbh_list_empty(fsevents)) {
        free(fsevents);
        return NULL;
    }

    return fsevents;
}

/* Remove the ns.xattrs.path on all the children of an entry.
 *
 * @param entry
 *
 * @return true if we have remove the ns.xattrs.path of at least one child,
 *         false otherwise.
 */
bool
remove_children_path(struct rbh_backend *backend, struct rbh_fsentry *entry)
{
    struct rbh_mut_iterator *children;
    struct rbh_iterator *update_iter;
    struct rbh_mut_iterator *chunks;
    struct rbh_list_node *fsevents;
    struct rbh_sstack *stack;

    children = get_entry_children(backend, entry);

    stack = rbh_sstack_new(1 << 10);

    fsevents = build_fsevents_remove_path(children, stack);

    rbh_mut_iter_destroy(children);

    if (fsevents == NULL) {
        rbh_sstack_destroy(stack);
        return false;
    }

    update_iter = rbh_iter_list(fsevents,
                                offsetof(struct rbh_node_fsevent, link),
                                free_fsevents_list);

    /* XXX: the mongo backend tries to process all the fsevents at once in a
     *      single bulk operation, but a bulk operation is limited in size.
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
    rbh_sstack_destroy(stack);

    return true;
}
