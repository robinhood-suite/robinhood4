/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "alias.h"
#include "config.h"
#include "error.h"


struct alias_entry *aliases = NULL;
size_t alias_count = 0;

int
apply_alias(const char *alias_name, int *new_argc, char **new_argv[],
            int *argc, char *argv[])
{
    size_t i, j;
    struct rbh_value *options = NULL;
    size_t new_args_count = *argc;

    for (i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, alias_name) == 0) {
            options = aliases[i].value;
            break;
        }
    }

    if (!options) {
        fprintf(stderr, "Alias '%s' not found.\n", alias_name);
        return -1;
    }

    new_args_count += options->map.count * 2 - 2;

    char **extended_argv = calloc(new_args_count + 1, sizeof(char *));
    if (!extended_argv) {
        fprintf(stderr, "Memory allocation failed for new arguments.\n");
        return -1;
    }

    size_t idx = 0;
    for (j = 0; j < (*argc); j++) {
        if (strcmp(argv[j], "-a") == 0 || strcmp(argv[j], "--alias") == 0) {
            j++;
            continue;
        }

        if (strstr(argv[j], "rbh:") == argv[j])
            break;

        extended_argv[idx++] = argv[j];
    }

    for (size_t k = 0; k < options->map.count; k++) {
        if (options->map.pairs[k].key[0] == '-' &&
            options->map.pairs[k].key[1] == '-') {
        extended_argv[idx++] = strdup(options->map.pairs[k].key);
        extended_argv[idx++] = strdup(options->map.pairs[k].value->string);
        }
    }

    extended_argv[idx++] = argv[j++];

    for (size_t k = 0; k < options->map.count; k++) {
        if (options->map.pairs[k].key[0] == '-' &&
            options->map.pairs[k].key[1] != '-') {
        extended_argv[idx++] = strdup(options->map.pairs[k].key);
        extended_argv[idx++] = strdup(options->map.pairs[k].value->string);
        }
    }

    for (; j < (*argc); j++) {
        extended_argv[idx++] = argv[j];
    }

    extended_argv[idx] = NULL;

    *new_argc = (int)new_args_count;
    *new_argv = extended_argv;

    return 0;
}

int
load_aliases_from_config(void)
{
    struct rbh_value aliases_value = { 0 };
    enum key_parse_result rc;

    rc = rbh_config_find("ALIASES_KEY", &aliases_value, RBH_VT_MAP);
    if (rc == KPR_ERROR) {
        fprintf(stderr, "Error loading aliases from configuration.\n");
        return -1;
    }
    if (rc == KPR_NOT_FOUND) {
        fprintf(stderr, "No aliases found in configuration.\n");
        return 0;
    }

    alias_count = aliases_value.map.count;
    if (alias_count == 0) {
        fprintf(stderr, "No alias entries found.\n");
        return 0;
    }
    aliases = calloc(alias_count, sizeof(struct alias_entry));

    for (int i = 0; i < alias_count; i++) {
        aliases[i].name = aliases_value.map.pairs[i].key;
        aliases[i].value = malloc(sizeof(struct rbh_value));

        if (!aliases[i].value) {
            fprintf(stderr, "Memory allocation failed for alias options.\n");
            return -1;
        }
        *aliases[i].value = *aliases_value.map.pairs[i].value;
    }

    return 0;
}

void
rbh_aliases_free(void)
{
    if (aliases) {
        for (size_t i = 0; i < alias_count; i++) {
            if (aliases[i].value) {
                free(aliases[i].value);
            }
        }
        free(aliases);
        aliases = NULL;
        alias_count = 0;
    }
}

