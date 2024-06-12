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

#include "robinhood/config.h"
#include "robinhood/serialization.h"

struct rbh_config {
    FILE *file;
    yaml_parser_t parser;
    char *config_file;
    bool parser_initialized;
};

static struct rbh_config *config;

enum key_parse_result {
    KPR_FOUND,
    KPR_NOT_FOUND,
    KPR_ERROR,
};

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

    if (config->file)
        fclose(config->file);

    free(config->config_file);

    if (config->parser_initialized)
        yaml_parser_delete(&config->parser);

    free(config);
    config = NULL;
}

static enum key_parse_result
_rbh_config_find(const char *key)
{
    const char *current_key;
    yaml_event_type_t type;
    yaml_event_t event;

next_line:
    if (!yaml_parser_parse(&config->parser, &event)) {
        fprintf(stderr, "Failed to parse event in _rbh_config_find\n");
        return KPR_ERROR;
    }

    switch (event.type) {
    case YAML_MAPPING_START_EVENT:
        /* Set the parser to the next key */
        yaml_event_delete(&event);

        if (!yaml_parser_parse(&config->parser, &event)) {
            fprintf(stderr, "Failed to parse event in _rbh_config_find\n");
            return KPR_ERROR;
        }

        break;
    case YAML_MAPPING_END_EVENT:
    case YAML_DOCUMENT_END_EVENT:
    case YAML_STREAM_END_EVENT:
        return KPR_NOT_FOUND;
    case YAML_SCALAR_EVENT:
        break;
    default:
        fprintf(stderr,
                "Found a key that is not a scalar event in _rbh_config_find\n");
        yaml_event_delete(&event);
        return KPR_ERROR;
    }

    if (!yaml_parse_string(&event, &current_key, NULL)) {
        fprintf(stderr, "Failed to parse the key in _rbh_config_find\n");
        yaml_event_delete(&event);
        return KPR_ERROR;
    }

    if (strcmp(current_key, key) == 0)
        return KPR_FOUND;

    yaml_event_delete(&event);

    if (!yaml_parser_parse(&config->parser, &event)) {
        fprintf(stderr, "Failed to parse event in _rbh_config_find\n");
        return KPR_ERROR;
    }

    type = event.type;
    yaml_event_delete(&event);

    switch (type) {
    case YAML_ALIAS_EVENT:
    case YAML_SCALAR_EVENT:
        break;
    case YAML_SEQUENCE_START_EVENT:
    case YAML_MAPPING_START_EVENT:
        if (!yaml_parser_skip(&config->parser, type)) {
            fprintf(stderr, "Failed to skip event in _rbh_config_find\n");
            return KPR_ERROR;
        }

        yaml_event_delete(&event);
        break;
    default:
        fprintf(stderr, "Invalid event found in _rbh_config_find\n");
        return KPR_ERROR;
    }

    goto next_line;
}

int
rbh_config_find(const char *_key, struct rbh_value *value)
{
    enum key_parse_result result;
    yaml_event_t event;
    char *subkey;
    char *key;

    value->type = -1;

    if (_key == NULL) {
        errno = EINVAL;
        return 1;
    }

    /* The configuration file wasn't opened, so consider there is no
     * configuration file to use, and let the user decide what to do.
     */
    if (config == NULL)
        return 0;

    key = strdup(_key);
    if (key == NULL)
        error(EXIT_FAILURE, ENOMEM, "strdup in rbh_config_find");

    subkey = strtok(key, "/");
    while (subkey != NULL) {
        result = _rbh_config_find(subkey);
        if (result == KPR_ERROR) {
            free(key);
            errno = EINVAL;
            return 1;
        } else if (result == KPR_NOT_FOUND) {
            fprintf(stderr, "Failed to find key '%s' in rbh_config_find\n",
                    _key);
            free(key);
            return 0;
        }

        subkey = strtok(NULL, "/");
    }

    free(key);

    if (!yaml_parser_parse(&config->parser, &event)) {
        fprintf(stderr, "Failed to parse event in rbh_config_find\n");
        return 1;
    }

    if (!parse_rbh_value(&config->parser, &event, value)) {
        fprintf(stderr, "Failed to parse value in rbh_config_find\n");
        return 1;
    }

    return 0;
}

struct rbh_config *
get_rbh_config()
{
    return config;
}

void
set_rbh_config(struct rbh_config *new_config)
{
    config = new_config;
}
