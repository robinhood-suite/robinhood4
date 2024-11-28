/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "alias.h"

static struct rbh_sstack *aliases_stack;
static struct rbh_value_map *aliases;

static bool process_alias(int *argc, char ***argv, int index,
                          const char **history, size_t history_count);

static void alias_resolution(int *argc, char ***argv, size_t alias_index,
                             size_t argv_alias_index, const char **history,
                             size_t history_count);

static void __attribute__((destructor))
destroy_aliases_stack(void)
{
    if (aliases_stack)
        rbh_sstack_destroy(aliases_stack);
}

static void
handle_config_option(int argc, char *argv[], int index)
{
    if (index + 1 >= argc)
        error(EX_USAGE, EINVAL, "'--config' option requires a file");

    if (rbh_config_open(argv[index + 1]))
        error(EX_USAGE, errno,
              "Failed to open configuration file '%s'", argv[index + 1]);
}

void
import_configuration_file(int *argc, char ***argv)
{
    const char* default_config = "/etc/robinhood4/rbh_conf.yaml";

    for (int i = 0; i < *argc; i++) {
        if (strcmp((*argv)[i], "-c") == 0 ||
            strcmp((*argv)[i], "--config") == 0) {
            handle_config_option(*argc, *argv, i);

            for (int j = i; j < *argc - 2; j++) {
                (*argv)[j] = (*argv)[j + 2];
            }

            *argc -= 2;

            return;
        }
    }

    if (rbh_config_open(default_config)) {
        error(EX_USAGE, errno, "Failed to open default configuration file '%s'",
              default_config);
    }
}

static int
load_aliases_from_config(void)
{
    struct rbh_value value = { 0 };
    struct rbh_value_pair *pairs;
    enum key_parse_result rc;
    size_t stack_size = 1 << 7;

    rc = rbh_config_find("alias", &value, RBH_VT_MAP);
    if (rc == KPR_ERROR) {
        fprintf(stderr, "Error reading aliases from configuration.\n");
        return -1;
    }

    if (rc == KPR_NOT_FOUND) {
        fprintf(stderr, "No aliases found in configuration.\n");
        aliases = NULL;
        return 0;
    }

    if (value.map.count == 0) {
        fprintf(stderr, "An empty alias section found in configuration.\n");
        aliases = NULL;
        return 0;
    }

    aliases_stack = rbh_sstack_new(stack_size);
    if (aliases_stack == NULL) {
        fprintf(stderr, "Failed to create alias stack.\n");
        return -1;
    }

    aliases = rbh_sstack_push(aliases_stack, NULL, sizeof(*aliases));
    if (aliases == NULL) {
        fprintf(stderr, "Failed to push alias structure.\n");
        return -1;
    }

    pairs = rbh_sstack_push(aliases_stack, NULL,
                            value.map.count * sizeof(*pairs));
    if (pairs == NULL) {
        fprintf(stderr, "Failed to push alias pairs.\n");
        return -1;
    }

    for (int i = 0; i < value.map.count; i++) {
        struct rbh_value *alias_value;

        pairs[i].key = value.map.pairs[i].key;

        alias_value = rbh_sstack_push(aliases_stack, NULL,
                                      sizeof(*alias_value));
        if (alias_value == NULL) {
            return -1;
        }

        alias_value->type = RBH_VT_STRING;
        alias_value->string = value.map.pairs[i].value->string;
        pairs[i].value = alias_value;
    }

    aliases->pairs = pairs;
    aliases->count = value.map.count;

    return 0;
}

static int
count_tokens_by_space(const char *str)
{
    char *copy = strdup(str);
    int count = 0;
    char *token = strtok(copy, " ");

    while (token != NULL) {
        count++;
        token = strtok(NULL, " ");
    }

    free(copy);
    return count;
}

static int
add_args_to_argv(char **argv_dest, int dest_index, const char *args)
{
    char *copy = strdup(args);
    char *arg = strtok(copy, " ");
    while (arg != NULL) {
        argv_dest[dest_index++] = strdup(arg);
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

static bool
is_alias_in_history(const char **history, size_t history_count,
                    const char *alias_name)
{
    for (size_t i = 0; i < history_count; i++) {
        if (strcmp(history[i], alias_name) == 0) {
            return true;
        }
    }
    return false;
}

static const char**
update_history(const char **history, size_t history_count,
               const char *alias_name)
{
    const char **new_history = rbh_sstack_push(aliases_stack, NULL,
                                               (history_count + 1) *
                                               sizeof(char *));
    if (!new_history) {
        fprintf(stderr, "Failed to allocate memory for history.\n");
        return NULL;
    }

    memcpy(new_history, history, history_count * sizeof(char *));
    new_history[history_count] = alias_name;
    return new_history;
}

static bool
process_alias(int *argc, char ***argv, int index, const char **history,
              size_t history_count)
{
    if (index + 1 >= *argc) {
        fprintf(stderr, "No alias name provided\n");
        return false;
    }

    const char *alias_name = (*argv)[index + 1];
    size_t alias_index = -1;

    for (size_t j = 0; j < aliases->count; j++) {
        if (strcmp(aliases->pairs[j].key, alias_name) == 0) {
            alias_index = j;
            break;
        }
    }

    if (alias_index != -1) {
        alias_resolution(argc, argv, alias_index, index, history,
                         history_count);
        return true;
    } else {
        fprintf(stderr, "Alias '%s' not found.\n", alias_name);
        return false;
    }
}

static void
alias_resolution(int *argc, char ***argv, size_t alias_index,
                 size_t argv_alias_index, const char **history,
                 size_t history_count)
{
    const char *alias_value = aliases->pairs[alias_index].value->string;
    const char **new_history;
    char **argv_temp;
    int temp_index;
    int new_argc;

    // Check if the alias has been used in the history (avoid circular alias)
    if (is_alias_in_history(history, history_count,
                            aliases->pairs[alias_index].key)) {
        fprintf(stderr, "Circular alias detected for '%s'.\n",
                aliases->pairs[alias_index].key);

        // Skip over the current alias in argv
        copy_args(*argv, *argv, argv_alias_index + 2, *argc, argv_alias_index);
        *argc -= 2;
        return;
    }

    // Update history with the new alias
    new_history = update_history(history, history_count,
                                 aliases->pairs[alias_index].key);
    history_count++;

    new_argc = *argc - 2 + count_tokens_by_space(alias_value);
    argv_temp = rbh_sstack_push(aliases_stack, NULL,
                                       new_argc * sizeof(char *));
    if (!argv_temp) {
        fprintf(stderr, "Failed to allocate memory for argv_temp.\n");
        return;
    }

    temp_index = 0;

    // Copy arguments before the alias into argv_temp
    temp_index = copy_args(argv_temp, *argv, 0, argv_alias_index, temp_index);

    // Add alias tokens to argv_temp
    temp_index = add_args_to_argv(argv_temp, temp_index, alias_value);

    // Copy remaining arguments after the alias into argv_temp
    temp_index = copy_args(argv_temp, *argv, argv_alias_index + 2,
                           *argc, temp_index);

    *argc = temp_index;
    *argv = argv_temp;

    // Handle resolution of nested aliases (recursively)
    for (int i = 0; i < *argc; i++) {
        if (strcmp((*argv)[i], "--alias") != 0)
            continue;

        if (!process_alias(argc, argv, i, new_history, history_count)) {
            exit(EXIT_FAILURE);
        }
    }
}

void
apply_aliases(int *argc, char ***argv)
{
    if (load_aliases_from_config() != 0) {
        fprintf(stderr, "Failed to load aliases from configuration.\n");
        return;
    }

    if (!aliases || aliases->count == 0) {
        fprintf(stderr, "No aliases to display.\n");
        return;
    }

    for (int i = 0; i < *argc; i++) {
        if (strcmp((*argv)[i], "--alias") != 0)
            continue;

        const char **empty_history = NULL;

        if (!process_alias(argc, argv, i, empty_history, 0))
            return;

        i = -1;
    }
}
