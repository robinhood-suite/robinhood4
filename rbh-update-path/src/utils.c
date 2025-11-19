/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "utils.h"

#include <robinhood.h>

struct rbh_mut_iterator *
get_entries(struct rbh_backend *backend, struct rbh_filter *filter)
{
    const struct rbh_filter_projection proj = {
        .fsentry_mask = RBH_FP_ID | RBH_FP_PARENT_ID | RBH_FP_NAME |
                        RBH_FP_STATX,
        .statx_mask = RBH_STATX_TYPE,
    };
    const struct rbh_filter_options option = {0};
    const struct rbh_filter_output output = {
        .type = RBH_FOT_PROJECTION,
        .projection = proj,
    };
    struct rbh_mut_iterator *fsentries;

    fsentries = rbh_backend_filter(backend, filter, &option, &output, NULL);
    if (fsentries == NULL) {
        if (errno == RBH_BACKEND_ERROR)
            error(EXIT_FAILURE, 0, "%s", rbh_backend_error);
        else
            error(EXIT_FAILURE, errno,
                  "failed to execute filter on backend '%s'", backend->name);
    }

    free(filter);

    return fsentries;
}

struct rbh_fsevent *
generate_fsevent_ns_xattrs(struct rbh_fsentry *entry, struct rbh_value *value)
{
    struct rbh_value_map xattrs;
    struct rbh_fsevent *fsevent;
    struct rbh_value_pair pair;

    pair.key = "path";
    pair.value = value;

    xattrs.pairs = &pair;
    xattrs.count = 1;

    fsevent = rbh_fsevent_ns_xattr_new(&entry->id, &xattrs, &entry->parent_id,
                                       entry->name);

    if (fsevent == NULL)
        error(EXIT_FAILURE, errno, "failed to generate fsevent");

    return fsevent;
}

struct rbh_fsevent *
generate_fsevent_update_path(struct rbh_fsentry *entry,
                             const struct rbh_value *value_path)
{
    struct rbh_fsevent *fsevent;
    struct rbh_value value;
    char *format;
    char *path;

    if (strcmp(value_path->string, "/") == 0)
        format = "%s%s";
    else
        format = "%s/%s";

    if (asprintf(&path, format, value_path->string, entry->name) == -1)
        error(EXIT_FAILURE, errno, "failed to create the path");

    value.type = RBH_VT_STRING;
    value.string = path;

    fsevent = generate_fsevent_ns_xattrs(entry, &value);

    free(path);

    return fsevent;
}

#define MIN_VALUES_SSTACK_ALLOC (1 << 6)

static __thread struct rbh_sstack *stack =  NULL;

__attribute__((destructor))
void
free_stack(void)
{
    if (stack)
        rbh_sstack_destroy(stack);
}

struct rbh_node_data {
    void *data;
    struct rbh_list_node link;
};

void
add_data_list(struct rbh_list_node *list, void *data)
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

struct rbh_mut_iterator *
_rbh_mut_iter_list(struct rbh_list_node *list)
{
    struct rbh_iterator *list_iter;

    list_iter = rbh_iter_list(list, offsetof(struct rbh_node_data, link),
                              free_data_list);

    return (struct rbh_mut_iterator *) new_iter_list(list_iter);
}

struct rbh_iterator *
_rbh_iter_list(struct rbh_list_node *list)
{
    struct rbh_iterator *list_iter;

    list_iter = rbh_iter_list(list, offsetof(struct rbh_node_data, link),
                              free_data_list);

    return new_iter_list(list_iter);
}

int
chunkify_update(struct rbh_iterator *iter, struct rbh_backend *backend)
{
    struct rbh_mut_iterator *chunks;

    /* XXX: the mongo backend tries to process all the fsevents at once in a
     *      single bulk operation, but a bulk operation is limited in size.
     *
     * Splitting `fsevents' into fixed-size sub-iterators solves this.
     */
    chunks = rbh_iter_chunkify(iter, RBH_ITER_CHUNK_SIZE);
    if (chunks == NULL)
        return -1;

    do {
        struct rbh_iterator *chunk = rbh_mut_iter_next(chunks);
        int save_errno;
        ssize_t count;

        if (chunk == NULL) {
            if (errno == ENODATA)
                break;
            return -1;
        }

        count = rbh_backend_update(backend, chunk);
        save_errno = errno;
        rbh_iter_destroy(chunk);
        if (count < 0) {
            errno = save_errno;
            return -1;
        }
    } while (true);

    rbh_mut_iter_destroy(chunks);

    return 0;
}
