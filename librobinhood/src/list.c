/* This file is part of Robinhood 4
 * Copyright (C) 2023 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <robinhood/list.h>

bool
rbh_list_empty(struct rbh_list_node *list)
{
    return list->next == list;
}

void
rbh_list_init(struct rbh_list_node *list)
{
    list->next = list;
    list->prev = list;
}

void
rbh_list_add_tail(struct rbh_list_node *list, struct rbh_list_node *node)
{
    list->prev->next = node;
    node->next = list;
    node->prev = list->prev;
    list->prev = node;
}

void
rbh_list_add(struct rbh_list_node *list, struct rbh_list_node *node)
{
    list->next->prev = node;
    node->next = list->next;
    node->prev = list;
    list->next = node;
}

void
rbh_list_splice_tail(struct rbh_list_node *list1, struct rbh_list_node *list2)
{
    struct rbh_list_node *last = list1->prev;

    list1->prev = list2->prev;
    last->next = list2->next;

    list1->prev->next = list1;
    last->next->prev = last;
}

void
rbh_list_del(struct rbh_list_node *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;
}
