/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <robinhood/fsevent.h>
#include <robinhood/sstack.h>
#include <robinhood/statx.h>
#include <robinhood/utils.h>

#include "source.h"
#include "utils.h"

__thread struct rbh_sstack *source_stack;

static void __attribute__((destructor))
destroy_source_stack(void)
{
    if (source_stack)
        rbh_sstack_destroy(source_stack);
}

void
flush_source_stack()
{
    while (true) {
        size_t readable;

        rbh_sstack_peek(source_stack, &readable);
        if (readable == 0)
            break;

        rbh_sstack_pop(source_stack, readable);
    }

    rbh_sstack_shrink(source_stack);
}

struct rbh_value_pair *
build_pair(const char *key, struct rbh_value *(*part_builder)(void *),
           void *part_builder_arg)
{
    const struct rbh_value_pair PAIR[] = {
        {
            .key = key,
            .value = part_builder != NULL ?
                part_builder(part_builder_arg) :
                NULL,
        },
    };
    struct rbh_value_pair *pair;

    if (PAIR[0].value == NULL && part_builder != NULL)
        return NULL;

    pair = RBH_SSTACK_PUSH(source_stack, NULL, sizeof(*pair));
    memcpy(pair, PAIR, sizeof(*pair));

    return pair;
}

struct rbh_value *
build_empty_map(void *arg)
{
    const char *key = (const char *)arg;
    const struct rbh_value_map MAP = {
        .count = 1,
        .pairs = build_pair(key, NULL, NULL),
    };
    const struct rbh_value ENRICH = {
        .type = RBH_VT_MAP,
        .map = MAP,
    };
    struct rbh_value *enrich;

    return RBH_SSTACK_PUSH(source_stack, &ENRICH, sizeof(*enrich));
}

struct rbh_value_map
build_enrich_map(struct rbh_value *(*part_builder)(void *),
                 void *part_builder_arg)
{
    const struct rbh_value_map ENRICH = {
        .count = 1,
        .pairs = build_pair("rbh-fsevents", part_builder, part_builder_arg),
    };

    return ENRICH;
}

void *
source_stack_alloc(const void *data, size_t size)
{
    return RBH_SSTACK_PUSH(source_stack, data, size);
}

void
initialize_source_stack(size_t stack_size)
{
    source_stack = rbh_sstack_new(stack_size);
}
