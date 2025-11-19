/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#ifndef RBH_ITER_CHUNK_SIZE
# define RBH_ITER_CHUNK_SIZE (1 << 12)
#endif

struct rbh_node_iterator {
    struct rbh_mut_iterator *iterator;
    struct rbh_list_node link;
};

static inline void
add_iterator(struct rbh_list_node *list, struct rbh_mut_iterator *iterator)
{
    struct rbh_node_iterator *node = xmalloc(sizeof(*node));

    node->iterator = iterator;
    rbh_list_add_tail(list, &node->link);
}

static inline struct rbh_mut_iterator *
get_iterator(struct rbh_list_node *list)
{
    struct rbh_mut_iterator *iterator;
    struct rbh_node_iterator *node;

    if (rbh_list_empty(list))
        return NULL;

    node = rbh_list_first(list, struct rbh_node_iterator, link);
    rbh_list_del(&node->link);

    iterator = node->iterator;
    free(node);

    return iterator;
}

struct rbh_mut_iterator *
get_entries(struct rbh_backend *backend, struct rbh_filter *filter);

struct rbh_fsevent *
generate_fsevent_ns_xattrs(struct rbh_fsentry *entry, struct rbh_value *value);

struct rbh_fsevent *
generate_fsevent_update_path(struct rbh_fsentry *entry,
                             const struct rbh_value *value_path);

void
add_data_list(struct rbh_list_node *list, void *data);

struct rbh_mut_iterator *
_rbh_mut_iter_list(struct rbh_list_node *list);

struct rbh_iterator *
_rbh_iter_list(struct rbh_list_node *list);
