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
#include <sysexits.h>
#include "robinhood/alias.h"
#include "robinhood/config.h"
#include "robinhood/stack.h"
#include "robinhood/sstack.h"
#include "robinhood/utils.h"

static struct rbh_sstack *aliases_stack;
static struct rbh_stack *history_stack;
static struct rbh_value_map *aliases;

static void __attribute__((destructor))
destroy_aliases_stack(void)
{
    if (aliases_stack)
        rbh_sstack_destroy(aliases_stack);
    if (history_stack)
        rbh_stack_destroy(history_stack);
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
    history_stack = rbh_stack_new(stack_size);
    if (!history_stack) {
        fprintf(stderr, "Failed to create history stack.\n");
        return -1;
    }

    return 0;
}

static int
add_args_to_argv(char **argv_dest, int dest_index, const char *args)
{
    char *copy = strdup(args);
    char *arg = strtok(copy, " ");

    while (arg != NULL) {
        char *stack = rbh_sstack_push(aliases_stack, arg, strlen(arg) + 1);
        if (stack == NULL) {
            free(copy);
            return dest_index;
        }
        argv_dest[dest_index++] = stack;
        arg = strtok(NULL, " ");
    }
    free(copy);

    return dest_index;
}

static int
copy_args(char **dest, char **src, int start, int end, int dest_index)
{
    for (int i = start; i < end; i++) {
        dest[dest_index++] = src[i];
    }

    return dest_index;
}

static void
process_args(int *argc, char ***argv) {
    char **new_argv = NULL;
    int new_argc = 0;

    for (int i = 0; i < *argc; i++) {
        if (strcmp((*argv)[i], "--alias") == 0 && i + 1 < *argc) {
            char *aliases = strtok((*argv)[++i], ",");
            while (aliases != NULL) {
                char *alias = rbh_sstack_push(aliases_stack, "--alias",
                                              strlen("--alias") + 1);
                if (alias == NULL) {
                    fprintf(stderr, "Failed to push '--alias' to stack\n");
                    exit(EXIT_FAILURE);
                }

                char *alias_value = rbh_sstack_push(aliases_stack, aliases,
                                                    strlen(aliases) + 1);
                if (alias_value == NULL) {
                    fprintf(stderr, "Failed to push alias '%s' to stack\n",
                            aliases);
                    exit(EXIT_FAILURE);
                }

                new_argv = rbh_sstack_push(aliases_stack, new_argv,
                                           (new_argc + 2) * sizeof(char *));
                if (new_argv == NULL) {
                    fprintf(stderr, "Failed to push new_argv to stack\n");
                    exit(EXIT_FAILURE);
                }
                new_argv[new_argc++] = alias;
                new_argv[new_argc++] = alias_value;

                aliases = strtok(NULL, ",");
            }
        } else {
            char *arg = rbh_sstack_push(aliases_stack, (*argv)[i],
                                        strlen((*argv)[i]) + 1);
            if (arg == NULL) {
                fprintf(stderr, "Failed to push argument '%s' to stack\n",
                        (*argv)[i]);
                exit(EXIT_FAILURE);
            }

            new_argv = rbh_sstack_push(aliases_stack, new_argv,
                                       (new_argc + 1) * sizeof(char *));
            if (new_argv == NULL) {
                fprintf(stderr, "Failed to push new_argv to stack\n");
                exit(EXIT_FAILURE);
            }
            new_argv[new_argc++] = arg;
        }
    }

    *argc = new_argc;
    *argv = new_argv;
}

static int
alias_resolution(int *argc, char ***argv, size_t alias_index,
                 size_t argv_alias_index)
{
    const char *alias_value;
    bool alias_found;
    char **argv_temp;
    size_t readable;
    int parse_range;
    int temp_index;
    int new_argc;
    char *data;

    alias_value = aliases->pairs[alias_index].value->string;
    new_argc = *argc - 2 + count_char_separated_values(alias_value, ' ');
    temp_index = 0;

    data = rbh_stack_peek(history_stack, &readable);
    for (size_t i = 0; i < readable;) {
        if (strcmp(data + i, aliases->pairs[alias_index].key) == 0) {
            fprintf(stderr, "[ERROR] Infinite loop detected for alias '%s'."
                    "Execution stopped.\n", aliases->pairs[alias_index].key);
            exit(EXIT_FAILURE);
        }
        i += strlen(data + i) + 1;
    }

    rbh_stack_push(history_stack, aliases->pairs[alias_index].key,
                   strlen(aliases->pairs[alias_index].key) + 1);

    argv_temp = malloc(new_argc * sizeof(char *));
    if (!argv_temp) {
        fprintf(stderr, "Failed to allocate memory for argv_temp.\n");
        return -1;
    }

    temp_index = copy_args(argv_temp, *argv, 0, argv_alias_index, temp_index);

    temp_index = add_args_to_argv(argv_temp, temp_index, alias_value);

    parse_range = temp_index - argv_alias_index;

    temp_index = copy_args(argv_temp, *argv, argv_alias_index + 2, *argc,
                           temp_index);

    *argc = temp_index;
    *argv = argv_temp;

    process_args(argc,argv);

    do {
        alias_found = false;
        for (int i = argv_alias_index; i < argv_alias_index + parse_range; i++) {
            if (i < *argc && strcmp((*argv)[i], "--alias") == 0) {
                const char *alias_name = (*argv)[i + 1];
                int alias_index_for_res = -1;

                for (int j = 0; j < aliases->count; j++) {
                    if (strcmp(aliases->pairs[j].key, alias_name) == 0) {
                        alias_index_for_res = j;
                        break;
                    }
                }

                if (alias_index_for_res != -1) {
                    int modified_elements;
                    modified_elements = alias_resolution(argc, argv,
                                                         alias_index_for_res,
                                                         i);

                    if (modified_elements != 0)
                        parse_range += modified_elements;

                    alias_found = true;
                    break;
                } else {
                    fprintf(stderr, "Alias '%s' not found in conf file\n",
                            alias_name);
                    exit(EXIT_FAILURE);
                }
            }
        }
    } while (alias_found);

    rbh_stack_pop(history_stack, strlen(aliases->pairs[alias_index].key) + 1);

    return parse_range - 2 ;
}

void
apply_aliases(int *argc, char ***argv)
{
    bool found;

    if (load_aliases_from_config() != 0) {
        fprintf(stderr, "Failed to load aliases from configuration.\n");
        return;
    }

    process_args(argc,argv);

    do {
        found = false;
        for (int i = 0; i < *argc; i++) {
            if (strcmp((*argv)[i], "--alias") == 0) {
                const char *alias_name = (*argv)[i + 1];
                size_t alias_index = -1;

                for (int j = 0; j < aliases->count; j++) {
                    if (strcmp(aliases->pairs[j].key, alias_name) == 0) {
                        alias_index = j;
                        break;
                    }
                }

                if (alias_index != -1) {
                    alias_resolution(argc, argv, alias_index, i);
                    found = true;
                    break;
                } else {
                    fprintf(stderr, "Alias '%s' not found in conf file\n",
                            alias_name);
                    exit(EXIT_FAILURE);
                }
            }
        }
    } while (found);
}

void
display_resolved_argv(char* name, int *argc, char ***argv)
{

    apply_aliases(argc, argv);

    printf("Command after alias resolution:\n");
    if (name != NULL)
        printf("%s ", name);
    for (int i = 0; i < *argc; i++) {
        printf("%s ", (*argv)[i]);
    }
    printf("\n");
}
