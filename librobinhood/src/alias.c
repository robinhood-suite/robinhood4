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
#include "robinhood/utils.h"

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

static void
alias_resolution(int *argc, char ***argv, size_t alias_index,
                 size_t argv_alias_index)
{
    /* Retrieve the alias string from the configuration */
    const char* alias_value = aliases->pairs[alias_index].value->string;

    int new_argc = *argc - 2 + count_char_separated_values(alias_value, ' ');
    int temp_index = 0;
    char **argv_temp;
    char *alias_copy;
    char *arg;

    /* Allocate memory for argv_temp, which will hold the arguments after alias
     * resolution
     */
    argv_temp = rbh_sstack_push(aliases_stack, NULL, new_argc * sizeof(char *));
    if (!argv_temp) {
        fprintf(stderr, "Failed to allocate memory for argv_temp.\n");
        return;
    }

    /* Copy the arguments before the alias into argv_temp */
    for (size_t i = 0; i < argv_alias_index; i++) {
        argv_temp[temp_index++] = (*argv)[i];
    }

    /* Duplicate the alias into a new string for manipulation */
    alias_copy = strdup(alias_value);
    if (!alias_copy) {
        fprintf(stderr, "Failed to duplicate alias_value.\n");
        return;
    }

    /* Use strtok to split the alias into tokens separated by spaces
     * Each alias has this syntax in the config file:
     * name: <list of alias arguments>
     */
    arg = strtok(alias_copy, " ");
    while (arg != NULL) {
        argv_temp[temp_index++] = rbh_sstack_push(aliases_stack, NULL,
                                                  strlen(arg) + 1);
        if (!argv_temp[temp_index - 1]) {
            fprintf(stderr, "Failed to allocate memory for token.\n");
            free(alias_copy);
            return;
        }
        strcpy(argv_temp[temp_index - 1], arg);
        arg = strtok(NULL, " ");
    }

    /* Copy the remaining arguments into argv_temp after the alias */
    for (size_t i = argv_alias_index + 2; i < *argc; i++)
        argv_temp[temp_index++] = (*argv)[i];

    *argc = new_argc;
    *argv = argv_temp;

    free(alias_copy);
}

void
apply_aliases(int *argc, char ***argv)
{
    if (load_aliases_from_config() != 0) {
        fprintf(stderr, "Failed to load aliases from configuration.\n");
        return;
    }

    for (int i = 0; i < *argc; i++) {
        const char* alias_name;
        size_t alias_index;

        if (strcmp((*argv)[i], "--alias") != 0)
            continue;

        if (i + 1 >= *argc) {
            fprintf(stderr, "No alias name provided\n");
            return;
        }

        alias_name = (*argv)[i + 1];
        alias_index = -1;

        for (size_t j = 0; j < aliases->count; j++) {
            if (strcmp(aliases->pairs[j].key, alias_name) == 0) {
                alias_index = j;
                break;
            }
        }

        if (alias_index != -1) {
            alias_resolution(argc, argv, alias_index, i);

            printf("Updated argv after resolution:\n");
            for (int k = 0; k < *argc; k++) {
                printf("argv[%d]: %s\n", k, (*argv)[k]);
            }

            i = -1; /* reset the index of loop after an alias has been
                     * resolved
                     */
        } else {
            fprintf(stderr, "Alias '%s' not found.\n", alias_name);
            exit(EXIT_FAILURE);
        }
    }
}
