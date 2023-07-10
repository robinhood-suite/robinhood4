/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef ROBINHOOD_LIST_H
#define ROBINHOOD_LIST_H

/**
 * @file
 *
 */

#include <stdbool.h>
#include <stddef.h>

struct rbh_list_node {
    struct rbh_list_node *next;
    struct rbh_list_node *prev;
};

#define rbh_list_entry(ptr, type, member)                 \
    ({                                                    \
      (type *) ((char *) (ptr) - offsetof(type, member)); \
    })

#define rbh_list_first(list, type, elem) \
    rbh_list_entry((list)->next, type, elem)

#define rbh_list_next(node, type, elem) \
    rbh_list_entry((node)->next, type, elem)

bool
rbh_list_empty(struct rbh_list_node *list);

void
rbh_list_init(struct rbh_list_node *list);

void
rbh_list_splice_tail(struct rbh_list_node *list1, struct rbh_list_node *list2);

void
rbh_list_add(struct rbh_list_node *list, struct rbh_list_node *node);

void
rbh_list_add_tail(struct rbh_list_node *list, struct rbh_list_node *node);

void
rbh_list_del(struct rbh_list_node *node);

#define rbh_list_foreach(list, node, member)                  \
    for (node = rbh_list_first(list, __typeof__(*node), member); \
         &node->member != (list);                                \
         node = rbh_list_next(&(node)->member, __typeof__(*node), member))

/* Iterate over the list such that elements can be removed during the iteration.
 */
#define rbh_list_foreach_safe(list, var, tmp, member)            \
    for (var = rbh_list_first(list, __typeof__(*var), member),   \
         tmp = rbh_list_next(&var->member, __typeof__(*var),     \
                              member);                           \
         &var->member != (list);                                 \
         var = tmp,                                              \
         tmp = rbh_list_next(&(var)->member, __typeof__(*var), member))

#endif
