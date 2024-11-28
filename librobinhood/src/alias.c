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
apply_aliases(void)
{
    if (load_aliases_from_config() != 0) {
        fprintf(stderr, "Failed to load aliases from configuration.\n");
        return;
    }

    if (!aliases || aliases->count == 0) {
        fprintf(stderr, "No aliases to display.\n");
        return;

    printf("List of aliases:\n");
    for (size_t i = 0; i < aliases->count; i++) {
        printf("  %s : %s\n", aliases->pairs[i].key,
               aliases->pairs[i].value->string);
    }
}
