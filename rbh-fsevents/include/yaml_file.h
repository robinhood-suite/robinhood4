/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

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
#include <robinhood/statx.h>

#include "source.h"

struct yaml_fsevent_iterator {
    struct rbh_iterator iterator;

    yaml_parser_t parser;
    bool exhausted;

    void *source_item; /* This can be a rbh_fsevent for file source, a
                        * rbh_iterator for Hestia, ...
                        */
};

struct source *
yaml_fsevent_init(FILE *file, const struct rbh_iterator iterator,
                  const struct source *source, void *source_item);

const void *yaml_source_iter_next(void *iterator);

void yaml_source_iter_destroy(void *iterator);

#endif
