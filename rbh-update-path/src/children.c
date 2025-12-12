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

        add_data_list(fsentries, child);
        fsevent = generate_fsevent_ns_xattrs(child, NULL);
        add_data_list(fsevents, fsevent);
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
