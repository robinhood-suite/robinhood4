/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <stdbool.h>

#include <robinhood/fsevent.h>
#include <yaml.h>

static inline void __attribute__((noreturn))
parser_error(yaml_parser_t *parser)
{
    error(EXIT_FAILURE, 0, "parser error: %s", parser->problem);
    __builtin_unreachable();
}

bool
parse_name(yaml_parser_t *parser, const char **name);

bool
parse_rbh_value_map(yaml_parser_t *parser, struct rbh_value_map *map);

bool
emit_fsevent(yaml_emitter_t *emitter, const struct rbh_fsevent *fsevent);

/* On success \p fsevent's fields may point at static memory.
 *
 * Therefore, successive calls to parse_fsevents() will invalidate previously
 * parsed fsevents. If one needs to eliminate pointers to static memory they
 * should clone the fsevent.
 */
bool
parse_fsevent(yaml_parser_t *parser, struct rbh_fsevent *fsevent);

#endif
