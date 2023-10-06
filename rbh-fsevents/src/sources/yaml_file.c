/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <stdlib.h>

#include <miniyaml.h>
#include <robinhood/fsevent.h>

#include "include/serialization.h"
#include "source.h"

#include "yaml_file.h"

void
yaml_fsevent_init(struct yaml_fsevent_iterator *fsevents, FILE *file,
                  const struct rbh_iterator iterator)
{
    yaml_event_t event;

    if (!yaml_parser_initialize(&fsevents->parser))
        error(EXIT_FAILURE, 0, "yaml_paser_initialize");

    yaml_parser_set_input_file(&fsevents->parser, file);
    yaml_parser_set_encoding(&fsevents->parser, YAML_UTF8_ENCODING);

    if (!yaml_parser_parse(&fsevents->parser, &event))
        parser_error(&fsevents->parser);

    assert(event.type == YAML_STREAM_START_EVENT);
    yaml_event_delete(&event);

    fsevents->iterator = iterator;
    fsevents->exhausted = false;
    fsevents->fsevent.type = 0;
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
