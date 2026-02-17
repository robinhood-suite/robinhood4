/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "robinhood/serialization.h"

bool
rbh_action_parameters2value_map(const char *parameters,
                                struct rbh_value_map *map)
{
    yaml_parser_t parser;
    yaml_event_t event;
    bool success;

    if (!parameters || !map)
        return false;

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)parameters,
                                 strlen(parameters));

    /* Skip STREAM_START and DOCUMENT_START events */
    for (int i = 0; i < 2; i++) {
        if (!yaml_parser_parse(&parser, &event)) {
            yaml_parser_delete(&parser);
            return false;
        }
        yaml_event_delete(&event);
    }

    /* Parse the map directly */
    success = parse_rbh_value_map(&parser, map, true);
    yaml_parser_delete(&parser);
    return success;
}
/*
bool
rbh_action_parameters2value_map(const char *parameters,
                                struct rbh_value_map *map)
{
    yaml_parser_t parser;
    yaml_event_t event;
    char *yaml_doc;
    bool success;
    size_t len;

    if (!parameters || !map)
        return false;

    len = strlen(parameters) + 5;
    yaml_doc = xmalloc(len);
    snprintf(yaml_doc, len, "---\n%s", parameters);

    yaml_parser_initialize(&parser);
    yaml_parser_set_input_string(&parser, (const unsigned char *)yaml_doc,
                                 strlen(yaml_doc));

    if (!yaml_parser_parse(&parser, &event))
        goto error;
    if (event.type != YAML_STREAM_START_EVENT) {
        yaml_event_delete(&event);
        goto error;
    }
    yaml_event_delete(&event);

    if (!yaml_parser_parse(&parser, &event))
        goto error;
    if (event.type != YAML_DOCUMENT_START_EVENT) {
        yaml_event_delete(&event);
        goto error;
    }
    yaml_event_delete(&event);

    success = parse_rbh_value_map(&parser, map, true);

    yaml_parser_delete(&parser);
    free(yaml_doc);
    return success;

error:
    yaml_parser_delete(&parser);
    free(yaml_doc);
    return false;
}*/
