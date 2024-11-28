/* This file is part of RobinHood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "alias.h"

static struct rbh_sstack *aliases_stack;
static struct rbh_value_map *aliases;

void
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
    const char* default_config = "/etc/default_conf.yaml";

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

int
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
        if (strcmp((*argv)[i], "-a") == 0 ||
            strcmp((*argv)[i], "--alias") == 0) {
            if (i + 1 < *argc) {
                const char* alias_name = (*argv)[i + 1];
                size_t alias_index = -1;

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

                    i = -1;
                } else {
                    fprintf(stderr, "Alias '%s' not found.\n", alias_name);
                    return;
                }

            } else {
                fprintf(stderr, "No alias name provided\n");
                return;
            }
        }
    }
}

int
count_tokens(const char *str)
{
    int count = 0;
    char *copy = strdup(str);
    char *token = strtok(copy, " ");
    while (token != NULL) {
        count++;
        token = strtok(NULL, " ");
    }
    free(copy);
    return count;
}

void
alias_resolution(int *argc, char ***argv, size_t alias_index,
                 size_t argv_alias_index)
{
    const char* alias_value = aliases->pairs[alias_index].value->string;

    int new_argc = *argc - 2 + count_tokens(alias_value);
    char **argv_temp = malloc(new_argc * sizeof(char *));

    int temp_index = 0;

    for (size_t i = 0; i < argv_alias_index; i++) {
        argv_temp[temp_index++] = (*argv)[i];
    }

    char *arg = strtok(strdup(alias_value), " ");
    while (arg != NULL) {
        argv_temp[temp_index++] = arg;
        arg = strtok(NULL, " ");
    }

    for (size_t i = argv_alias_index + 2; i < *argc; i++)
        argv_temp[temp_index++] = (*argv)[i];

    *argc = new_argc;
    *argv = argv_temp;
}
