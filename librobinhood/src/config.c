/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <robinhood/config.h>

struct rbh_config *
rbh_config_initialize(const char *config_file)
{
    struct rbh_config *config;
    yaml_event_t event;

    config = malloc(sizeof(*config));
    if (config == NULL)
        error(EXIT_FAILURE, errno, "malloc in rbh_config_initialize");

    if (!yaml_parser_initialize(&config->parser))
        error(EXIT_FAILURE, errno,
              "yaml_paser_initialize in rbh_config_initialize");

    config->file = fopen(config_file, "r");
    if (config->file == NULL)
        error(EXIT_FAILURE, errno, "fopen in rbh_config_initialize");

    yaml_parser_set_input_file(&config->parser, config->file);
    yaml_parser_set_encoding(&config->parser, YAML_UTF8_ENCODING);

    if (!yaml_parser_parse(&config->parser, &event))
        parser_error(&config->parser);

    assert(event.type == YAML_STREAM_START_EVENT);
    yaml_event_delete(&event);

    return config;
}
