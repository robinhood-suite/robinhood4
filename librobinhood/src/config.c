/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <robinhood/config.h>

enum key_parse_result {
    KPR_FOUND,
    KPR_NOT_FOUND,
    KPR_ERROR,
};

struct rbh_config *
rbh_config_initialize(const char *config_file)
{
    struct rbh_config *config;

    config = malloc(sizeof(*config));
    if (config == NULL)
        error(EXIT_FAILURE, errno, "malloc in rbh_config_initialize");

    config->file = fopen(config_file, "r");
    if (config->file == NULL)
        error(EXIT_FAILURE, errno, "fopen in rbh_config_initialize");

    rbh_config_reset(config);

    return config;
}

void
rbh_config_reset(struct rbh_config *config)
{
    yaml_event_t event;

    if (!yaml_parser_initialize(&config->parser))
        error(EXIT_FAILURE, errno,
              "yaml_paser_initialize in rbh_config_initialize");

    yaml_parser_set_input_file(&config->parser, config->file);
    yaml_parser_set_encoding(&config->parser, YAML_UTF8_ENCODING);

    if (!yaml_parser_parse(&config->parser, &event))
        parser_error(&config->parser);

    assert(event.type == YAML_STREAM_START_EVENT);
    yaml_event_delete(&event);

    if (!yaml_parser_parse(&config->parser, &event))
        parser_error(&config->parser);

    assert(event.type == YAML_DOCUMENT_START_EVENT);
    yaml_event_delete(&event);
}

static void
skip_to_next_event(struct rbh_config *config)
{
    enum yaml_event_type_t type;
    yaml_event_t event;

    if (!yaml_parser_parse(&config->parser, &event))
        parser_error(&config->parser);

    type = event.type;
    yaml_event_delete(&event);

    switch (event.type) {
    case YAML_ALIAS_EVENT:
    case YAML_SCALAR_EVENT:
        if (!yaml_parser_parse(&config->parser, &event))
            parser_error(&config->parser);

        yaml_event_delete(&event);
        return NULL;
    case YAML_SEQUENCE_START_EVENT:
    case YAML_MAPPING_START_EVENT:
        if (!yaml_parser_skip(&config->parser, type))
            parser_error(&config->parser);

        yaml_event_delete(&event);
        return NULL;
    }

}

static enum key_parse_result
_rbh_config_search(struct rbh_config *config, const char *key)
{
    enum yaml_event_type_t type;
    const char *current_key;
    yaml_event_t event;
    int save_errno;

    if (!yaml_parser_parse(&config->parser, &event))
        parser_error(&config->parser);

    assert(event.type == YAML_SCALAR_EVENT);

    if (!yaml_parse_string(&event, &current_key, NULL)) {
        save_errno = errno;
        yaml_event_delete(event);
        errno = save_errno;
        return KPR_ERROR;
    }

    yaml_event_delete(&event);

    if (strcmp(current_key, key) != 0)
        return KPR_FOUND;

    if (!yaml_parser_parse(&config->parser, &event))
        parser_error(&config->parser);

    type = event.type;
    yaml_event_delete(&event);

    switch (event.type) {
    case YAML_ALIAS_EVENT:
    case YAML_SCALAR_EVENT:
        if (!yaml_parser_parse(&config->parser, &event))
            parser_error(&config->parser);

        yaml_event_delete(&event);
        break;
    case YAML_SEQUENCE_START_EVENT:
    case YAML_MAPPING_START_EVENT:
        if (!yaml_parser_skip(&config->parser, type))
            parser_error(&config->parser);

        yaml_event_delete(&event);
        break;
    default:
        return KPR_ERROR;
    }

    return KPR_NOT_FOUND;
}

int
rbh_config_search(struct rbh_config *config, const char *_key,
                  yaml_event_t *event)
{
    enum key_parse_result result;
    char *search;
    char *key;

    if (_key == NULL) {
        errno = EINVAL;
        return 1;
    }

    key = strdup(_key);
    if (key == NULL)
        error(EXIT_FAILURE, ENOMEM, "strdup in rbh_config_search");

    search = strtok(key, "/");
    while (search != NULL) {
        result = _rbh_config_search(config, search);
        if (result == KPR_ERROR) {
            free(key);
            errno = EINVAL;
            return 1;
        } else if (result == KPR_NOT_FOUND) {
            free(key);
            return 0;
        }

        search = strtok(NULL, "/");
    }

    if (!yaml_parser_parse(&config->parser, event))
        parser_error(&config->parser);

    free(key);
    return 0;
}
