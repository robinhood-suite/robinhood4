/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>

#include <lustre/lustreapi.h>

#include <robinhood/fsevent.h>
#include <robinhood/sstack.h>
#include <robinhood/statx.h>

#include "source.h"
#include "utils.h"

__thread struct rbh_sstack *global_values;

static void __attribute__((destructor))
destroy_global_values(void)
{
    rbh_sstack_destroy(global_values);
}

void
flush_global_values()
{
    while (true) {
        size_t readable;

        rbh_sstack_peek(global_values, &readable);
        if (readable == 0)
            break;

        rbh_sstack_pop(global_values, readable);
    }

    rbh_sstack_shrink(global_values);
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

    pair = rbh_sstack_push(global_values, NULL, sizeof(*pair));
    if (pair == NULL)
        return NULL;
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

    return rbh_sstack_push(global_values, &ENRICH, sizeof(*enrich));
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

void
initialize_global_values(size_t stack_size)
{
    global_values = rbh_sstack_new(stack_size);
    if (!global_values)
        error(EXIT_FAILURE, errno,
              "rbh_sstack_new in initialize_global_values");
}
