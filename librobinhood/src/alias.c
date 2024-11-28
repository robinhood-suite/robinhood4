/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <error.h>
#include <sysexits.h>
#include "robinhood/alias.h"
#include "robinhood/config.h"
#include "robinhood/stack.h"
#include "robinhood/sstack.h"

static struct rbh_sstack *aliases_stack;
static struct rbh_value_map *aliases;

static void __attribute__((destructor))
destroy_aliases_stack(void)
{
    if (aliases_stack)
        rbh_sstack_destroy(aliases_stack);
}

static int
load_aliases_from_config(void)
{
    struct rbh_value value = { 0 };
    size_t stack_size = 1 << 10;
    enum key_parse_result rc;

    rc = rbh_config_find("alias", &value, RBH_VT_MAP);
    if (rc == KPR_ERROR) {
        fprintf(stderr, "Error reading aliases from configuration.\n");
        return -1;
    }

    if (rc == KPR_NOT_FOUND) {
        fprintf(stderr, "No aliases found in configuration.\n");
        aliases = NULL;
        return -1;
    }

    if (value.map.count == 0) {
        fprintf(stderr, "An empty alias section found in configuration.\n");
        aliases = NULL;
        return -1;
    }

    aliases_stack = rbh_sstack_new(stack_size);
    if (aliases_stack == NULL) {
        fprintf(stderr, "Failed to create alias stack.\n");
        return -1;
    }

    aliases = rbh_sstack_push(aliases_stack, &value.map, sizeof(value.map));
    if (aliases == NULL) {
        fprintf(stderr, "Failed to push alias structure.\n");
        return -1;
    }

    aliases->count = value.map.count;

    return 0;
}

void
apply_aliases(void)
{
    if (load_aliases_from_config() != 0) {
        fprintf(stderr, "Failed to load aliases from configuration.\n");
        return;
    }

    if (!aliases || aliases->count == 0) {
        fprintf(stderr, "No aliases to display.\n");
        return;
    }

    printf("List of aliases:\n");
    for (size_t i = 0; i < aliases->count; i++) {
        printf("  %s : %s\n", aliases->pairs[i].key,
               aliases->pairs[i].value->string);
    }
}
