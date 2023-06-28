/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <stdbool.h>

#include <robinhood/fsevent.h>
#include <yaml.h>

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
