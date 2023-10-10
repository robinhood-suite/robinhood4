/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef YAML_FILE_H
#define YAML_FILE_H

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>

#include <miniyaml.h>
#include <robinhood/fsevent.h>

#include "source.h"

struct yaml_fsevent_iterator {
    struct rbh_iterator iterator;

    struct rbh_fsevent fsevent;
    yaml_parser_t parser;
    bool exhausted;

    struct rbh_value_map additional_xattr;
};

struct source *
yaml_fsevent_init(FILE *file, const struct rbh_iterator iterator,
                  const struct source *source);

const void *yaml_source_iter_next(void *iterator);

void yaml_source_iter_destroy(void *iterator);

#endif
