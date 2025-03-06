/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <error.h>
#include <errno.h>
#include <miniyaml.h>
#include <stdlib.h>
#include <stdio.h>
#include <sysexits.h>

#include "robinhood/config.h"
#include "robinhood/serialization.h"

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
    if (config->file == NULL)
        goto free_config;

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

    if (config == NULL)
        return 0;

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
parse_and_set_value(yaml_parser_t *parser, struct rbh_value *value)
{
    yaml_event_t event;

    if (!yaml_parser_parse(&config->parser, &event)) {
        fprintf(stderr, "Failed to parse event in rbh_config_find\n");
        return KPR_ERROR;
    }

    if (!parse_rbh_value(&config->parser, &event, value)) {
        fprintf(stderr, "Failed to parse value in rbh_config_find\n");
        return KPR_ERROR;
    }

    return KPR_FOUND;
}

static int
skip_to_next_key()
{
    yaml_event_type_t type;
    yaml_event_t event;

    if (!yaml_parser_parse(&config->parser, &event)) {
        fprintf(stderr, "Failed to parse event in _rbh_config_find\n");
        return 1;
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
            return 1;
        }

        yaml_event_delete(&event);
        break;
    default:
        fprintf(stderr, "Invalid event found in _rbh_config_find\n");
        return 1;
    }

    return 0;
}

static enum key_parse_result
_rbh_config_find(char *key, struct rbh_value *value)
{
    const char *current_key;
    bool key_found = false;
    yaml_event_t event;
    char *subkey;
    int rc;

    subkey = strtok(key, "/");
    if (subkey == NULL)
        return parse_and_set_value(&config->parser, value);

next_line:
    if (!yaml_parser_parse(&config->parser, &event)) {
        fprintf(stderr, "Failed to parse event in _rbh_config_find\n");
        return KPR_ERROR;
    }

    /* Are we at the end of the file ? */
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
        return key_found ? KPR_FOUND : KPR_NOT_FOUND;
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

    if (strcmp(current_key, subkey) == 0) {
        enum key_parse_result rc;

        if (key_found) {
            fprintf(stderr,
                    "Duplicate key '%s' found in configuration file\n",
                    subkey);
            yaml_event_delete(&event);
            return KPR_ERROR;
        }

        rc = _rbh_config_find(NULL, value);
        if (rc == KPR_NOT_FOUND || rc == KPR_ERROR)
            return rc;

        key_found = true;

        goto next_line;
    }

    yaml_event_delete(&event);

    /* If the key found is not the one we search, directly skip to the next
     * same-level key.
     */

    rc = skip_to_next_key();
    if (rc)
        return KPR_ERROR;

    goto next_line;
}

static enum key_parse_result
find_in_config(const char *_key, struct rbh_value *value)
{
    enum key_parse_result result;
    char *key;

    if (_key == NULL) {
        errno = EINVAL;
        return KPR_ERROR;
    }

    key = strdup(_key);
    if (key == NULL)
        error(EXIT_FAILURE, ENOMEM, "strdup in rbh_config_find");

    result = _rbh_config_find(key, value);
    free(key);

    if (result == KPR_ERROR)
        errno = EINVAL;

    return result;
}

enum key_parse_result
rbh_config_find(const char *key, struct rbh_value *value,
                enum rbh_value_type expected_type)
{
    enum key_parse_result rc;

    /* The configuration file wasn't opened, so consider there is no
     * configuration file to use, and let the user decide what to do.
     */
    if (config == NULL)
        return KPR_NOT_FOUND;

    rc = find_in_config(key, value);
    rbh_config_reset();
    if (rc == KPR_ERROR)
        return rc;

    if (rc == KPR_FOUND) {
        if (value->type == expected_type)
            return KPR_FOUND;

        fprintf(stderr,
                "Expected the value of '%s' to be a '%s', found a '%s'\n",
                key, value_type2str(expected_type),
                value_type2str(value->type));
        errno = EINVAL;
        return KPR_ERROR;
    }

    if (expected_type != RBH_VT_STRING)
        // XXX: handle the different types when necessary
        return KPR_NOT_FOUND;

    value->type = RBH_VT_STRING;
    value->string = getenv(key);

    if (value->string == NULL)
        return KPR_NOT_FOUND;

    return KPR_FOUND;
}

struct rbh_config *
get_rbh_config()
{
    return config;
}

void
load_rbh_config(struct rbh_config *new_config)
{
    config = new_config;
}

const char *
rbh_config_get_string(const char *key, const char *default_string)
{
    struct rbh_value value = { 0 };
    enum key_parse_result rc;

    rc = rbh_config_find(key, &value, RBH_VT_STRING);
    if (rc == KPR_ERROR)
        return NULL;

    if (rc == KPR_NOT_FOUND)
        return default_string;

    return value.string;
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

static int
rbh_config_open_default()
{
    const char* default_config = "/etc/robinhood4.d/rbh_conf.yaml";

    int rc = rbh_config_open(default_config);

    if (rc && errno != ENOENT) {
        fprintf(stderr, "Warning: Failed to open default configuration file "
                "'%s': %s\n", default_config, strerror(errno));
        return 1;
    }

    return 0;
}

void
import_configuration_file(int *argc, char ***argv)
{
    for (int i = 0; i < *argc; i++) {
        if (strcmp((*argv)[i], "-c") == 0 ||
            strcmp((*argv)[i], "--config") == 0) {
            handle_config_option(*argc, *argv, i);

            return;
        }
    }

    rbh_config_open_default();
}

