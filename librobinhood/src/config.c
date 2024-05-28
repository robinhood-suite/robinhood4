/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <error.h>
#include <errno.h>
#include <miniyaml.h>
#include <stdlib.h>
#include <stdio.h>
#include <yaml.h>

#include <robinhood/config.h>

struct rbh_config {
    FILE *file;
    yaml_parser_t parser;
};

static struct rbh_config *config;

int
rbh_config_open(const char *config_file)
{
    struct rbh_config *config;
    yaml_event_t event;
    int save_errno;
    int type;

    config = calloc(1, sizeof(*config));
    if (config == NULL)
        error(EXIT_FAILURE, errno, "calloc in rbh_config_open");

    if (!yaml_parser_initialize(&config->parser)) {
        save_errno = errno;
        fprintf(stderr, "Failed to initialize parser in rbh_config_open\n");
        goto free_config;
    }

    config->file = fopen(config_file, "r");
    if (config->file == NULL) {
        save_errno = errno;
        fprintf(stderr, "Failed to open '%s' in rbh_config_open\n",
                config_file);
        goto free_config;
    }

    yaml_parser_set_input_file(&config->parser, config->file);
    yaml_parser_set_encoding(&config->parser, YAML_UTF8_ENCODING);

    if (!yaml_parser_parse(&config->parser, &event)) {
        save_errno = errno;
        fprintf(stderr,
                "Failed to parse initial token of '%s' in rbh_config_open\n",
                config_file);
        goto free_config;
    }

    type = event.type;
    yaml_event_delete(&event);

    if (type != YAML_STREAM_START_EVENT) {
        save_errno = errno;
        fprintf(stderr, "Initial token of '%s' is not a stream start event\n",
                config_file);
        goto free_config;
    }

    return 0;

free_config:
    rbh_config_free();
    errno = save_errno;

    return 1;
}

void
rbh_config_free()
{
    if (config == NULL)
        return;

    fclose(config->file);
    yaml_parser_delete(&config->parser);
    free(config);
    config = NULL;
}
