/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef SERIALIZATION_H
#define SERIALIZATION_H

#include <stdbool.h>

#include <robinhood/fsevent.h>
#include <yaml.h>

bool
emit_fsevent(yaml_emitter_t *emitter, const struct rbh_fsevent *fsevent);

bool
parse_fsevent(yaml_parser_t *parser, struct rbh_fsevent *fsevent);

#endif
