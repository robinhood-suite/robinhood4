/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "robinhood/serialization.h"
#include "robinhood/action.h"

const char *
action2string(enum rbh_action_type type)
{
    switch (type) {
    case RBH_ACTION_UNSET:
        return "unset";
    case RBH_ACTION_UNKNOWN:
        return "unknown";
    case RBH_ACTION_DELETE:
        return "delete";
    case RBH_ACTION_PRINT:
        return "log";
    default:
        return "invalid";
    }
}

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
