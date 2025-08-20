/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>

#include <miniyaml.h>
#include <robinhood/fsevent.h>
#include <robinhood/serialization.h>

#include "source.h"

#include "yaml_file.h"

struct file_source {
    struct source source;

    struct yaml_fsevent_iterator fsevents;
    FILE *file;
};

struct source *
yaml_fsevent_init(FILE *file, const struct rbh_iterator iterator,
                  const struct source *source, void *source_item)
{
    struct file_source *file_source;
    yaml_event_t event;

    file_source = malloc(sizeof(*file_source));
    if (file_source == NULL)
        error(EXIT_FAILURE, errno, "malloc in yaml_fsevent_init");

    if (!yaml_parser_initialize(&file_source->fsevents.parser))
        error(EXIT_FAILURE, errno,
              "yaml_paser_initialize in yaml_fsevent_init");

    yaml_parser_set_input_file(&file_source->fsevents.parser, file);
    yaml_parser_set_encoding(&file_source->fsevents.parser, YAML_UTF8_ENCODING);

    if (!yaml_parser_parse(&file_source->fsevents.parser, &event))
        parser_error(&file_source->fsevents.parser);

    assert(event.type == YAML_STREAM_START_EVENT);
    yaml_event_delete(&event);

    file_source->fsevents.iterator = iterator;
    file_source->fsevents.exhausted = false;
    file_source->fsevents.source_item = source_item;

    file_source->source = *source;
    file_source->file = file;

    return &file_source->source;
}

const void *
yaml_source_iter_next(void *iterator)
{
    struct file_source *source = iterator;

    return rbh_iter_next(&source->fsevents.iterator);
}

void
yaml_source_iter_destroy(void *iterator)
{
    struct file_source *source = iterator;

    rbh_iter_destroy(&source->fsevents.iterator);
    /* Ignore errors on close */
    fclose(source->file);
    free(source);
}
