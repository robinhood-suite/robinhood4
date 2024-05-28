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
    char *config_file;
    bool parser_initialized;
};

static struct rbh_config *config;

int
rbh_config_open(const char *config_file)
{
    int save_errno;

    config = calloc(1, sizeof(*config));
    if (config == NULL)
        error(EXIT_FAILURE, errno, "calloc in rbh_config_open");

    config->config_file = strdup(config_file);
    if (config->config_file == NULL) {
        fprintf(stderr, "Failed to duplicate '%s' in rbh_config_open\n",
                config_file);
        goto free_config;
    }

    config->file = fopen(config_file, "r");
    if (config->file == NULL) {
        fprintf(stderr, "Failed to open '%s' in rbh_config_open\n",
                config_file);
        goto free_config;
    }

    config->parser_initialized = false;

    if (rbh_config_reset())
        goto free_config;

    return 0;

free_config:
    save_errno = errno;
    rbh_config_free();
    errno = save_errno;

    return 1;
}

int
rbh_config_reset()
{
    yaml_event_t event;
    int type;

    if (config->parser_initialized) {
        yaml_parser_delete(&config->parser);
        config->parser_initialized = false;
        rewind(config->file);
    }

    if (!yaml_parser_initialize(&config->parser)) {
        fprintf(stderr, "Failed to initialize parser in rbh_config_reset\n");
        return 1;
    }

    config->parser_initialized = true;

    yaml_parser_set_input_file(&config->parser, config->file);
    yaml_parser_set_encoding(&config->parser, YAML_UTF8_ENCODING);

    if (!yaml_parser_parse(&config->parser, &event)) {
        fprintf(stderr,
                "Failed to parse initial token of '%s' in rbh_config_reset\n",
                config->config_file);
        goto free_config;
    }

    type = event.type;
    yaml_event_delete(&event);

    if (type != YAML_STREAM_START_EVENT) {
        fprintf(stderr, "Initial token of '%s' is not a stream start event\n",
                config->config_file);
        goto free_config;
    }

    if (!yaml_parser_parse(&config->parser, &event)) {
        fprintf(
            stderr,
            "Failed to parse first document token of '%s' in rbh_config_reset\n",
            config->config_file);
        goto free_config;
    }

    type = event.type;
    yaml_event_delete(&event);

    if (type != YAML_DOCUMENT_START_EVENT) {
        fprintf(stderr,
                "First document token of '%s' is not a document start event\n",
                config->config_file);
        goto free_config;
    }

    if (!yaml_parser_parse(&config->parser, &event)) {
        fprintf(
            stderr,
            "Failed to parse first mapping token of '%s' in rbh_config_reset\n",
            config->config_file);
        goto free_config;
    }

    type = event.type;
    yaml_event_delete(&event);

    if (type != YAML_MAPPING_START_EVENT) {
        fprintf(stderr,
                "First mapping token of '%s' is not a mapping start event\n",
                config->config_file);
        goto free_config;
    }

    return 0;

free_config:
    rbh_config_free();
    errno = EINVAL;

    return 1;
}

void
rbh_config_free()
{
    if (config == NULL)
        return;

    free(config->config_file);
    fclose(config->file);

    if (config->parser_initialized)
        yaml_parser_delete(&config->parser);

    free(config);
    config = NULL;
}
