/* SPDX-License-Identifier: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>

#include <robinhood/fsevent.h>

/*
 * This sstack is shared amongst the enrichers for easier use. Since some
 * enrichers use similar functions, they are defined here, and since they
 * require an sstack, we define one as a global variable.
 */
extern __thread struct rbh_sstack *source_stack;

void
flush_source_stack();

struct rbh_value_pair *
build_pair(const char *key, struct rbh_value *(*part_builder)(void *),
           void *part_builder_arg);

struct rbh_value *
build_empty_map(void *arg);

struct rbh_value_map
build_enrich_map(struct rbh_value *(*part_builder)(void *),
                 void *part_builder_arg);

void *
source_stack_alloc(const void *data, size_t size);

void
initialize_source_stack(size_t stack_size);
