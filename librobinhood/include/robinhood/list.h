/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef ROBINHOOD_LIST_H
#define ROBINHOOD_LIST_H

/**
 * @file
 *
 * Doubly linked list interface
 *
 * This interface uses the same principles as the Linux's list_head API.
 * If one wants to have a linked list of some structure `struct mystruct', one
 * needs to add an element of type `struct rbh_list_node' in the structure.
 *
 * For instance:
 *
 * struct mystruct {
 *     int value;
 *     struct rbh_list_node link;
 * };
 *
 * An empty list is simply a list that points to its head:
 *
 * struct rbh_list_node head;
 *
 * rbh_list_init(&head);
 *
 *                 +------+------+
 *          head:  | next | prev |
 *                 +---|--+---|--+
 *                 ^---+      |
 *                 ^----------+
 *
 * Adding an element to the end of the list:
 *
 * struct mystruct a = {
 *     .value = 42,
 * };
 *
 * rbh_list_add_tail(&head, &a.link);
 *
 *
 *                 +--------------------------------+
 *                 |          +----------+          |
 *                 v          |          v          |
 *                 +------+---|--+       +------+---|--+
 *          head:  | next | prev |    a: | next | prev |
 *                 +---|--+------+       +---|--+------+
 *                 ^   |                 ^   |
 *                 |   +-----------------+   |
 *                 +-------------------------+
 *
 * Since list elements are embedded inside the structure they link together, one
 * can retrieve the whole structure by using the right offset.
 *
 * The layout of mystruct would look something like this:
 *
 *           a_addr:       +-------+
 *                         | value |
 *           link_addr:    +-------+
 *                         | link  |
 *                         +-------+
 *
 * Given the address of an rbh_list_node linking to a structure of type
 * `struct mystruct', one can find the address of the whole structure:
 *
 *       a_addr = link_addr - offsetof(a, link)
 *
 * Which means that the following equality holds in our example:
 *
 *       rbh_list_entry(&a.link, struct mystruct, link) == &a
 */

#include <stdbool.h>
#include <stddef.h>

struct rbh_list_node {
    struct rbh_list_node *next;
    struct rbh_list_node *prev;
};

/* Given a pointer to a list element, return the pointer to the struct of type
 * \p type
 */
#define rbh_list_entry(ptr, type, member)                 \
    ({                                                    \
      (type *) ((char *) (ptr) - offsetof(type, member)); \
    })

/* Given the head of a list, return the first element */
#define rbh_list_first(list, type, elem) \
    rbh_list_entry((list)->next, type, elem)

/* Given a node of a list, return the next element. Cannot be called on a list
 * head.
 */
#define rbh_list_next(node, type, elem) \
    rbh_list_entry((node)->next, type, elem)

/* Whether a list is empty or not */
bool
rbh_list_empty(struct rbh_list_node *list);

/* Initialize an empty list. */
void
rbh_list_init(struct rbh_list_node *list);

/* Add list2 after list1 */
void
rbh_list_splice_tail(struct rbh_list_node *list1, struct rbh_list_node *list2);

/* Insert \p node at the beginning of \p list */
void
rbh_list_add(struct rbh_list_node *list, struct rbh_list_node *node);

/* Insert \p node at the end of \p list */
void
rbh_list_add_tail(struct rbh_list_node *list, struct rbh_list_node *node);

/* Remove \p node from the list */
void
rbh_list_del(struct rbh_list_node *node);

/* Iterate over the list. Elements cannot be removed during the iteration. */
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
