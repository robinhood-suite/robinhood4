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
#include "utils.h"

static const void *
yaml_fsevent_iter_next(void *iterator)
{
    struct yaml_fsevent_iterator *fsevents = iterator;
    struct rbh_fsevent *fsevent;
    yaml_event_type_t type;
    yaml_event_t event;

    fsevent = fsevents->source_item;

    if (fsevents->exhausted) {
        errno = ENODATA;
        return NULL;
    }

    if (!yaml_parser_parse(&fsevents->parser, &event))
        parser_error(&fsevents->parser);

    type = event.type;
    yaml_event_delete(&event);

    switch (type) {
    case YAML_DOCUMENT_START_EVENT:
        /* Remove any trace of the previous parsed fsevent */
        memset(fsevent, 0, sizeof(*fsevent));

        if (!parse_fsevent(&fsevents->parser, fsevent))
            parser_error(&fsevents->parser);

        if (!yaml_parser_parse(&fsevents->parser, &event))
            parser_error(&fsevents->parser);

        assert(event.type == YAML_DOCUMENT_END_EVENT);
        yaml_event_delete(&event);
        return fsevent;
    case YAML_STREAM_END_EVENT:
        fsevents->exhausted = true;
        errno = ENODATA;
        return NULL;
    default:
        error(EXIT_FAILURE, 0, "unexpected YAML event: type = %i", event.type);
        __builtin_unreachable();
    }
}

static void
yaml_fsevent_iter_destroy(void *iterator)
{
    struct yaml_fsevent_iterator *fsevents = iterator;

    yaml_parser_delete(&fsevents->parser);
}

static const struct rbh_iterator_operations YAML_FSEVENT_ITER_OPS = {
    .next = yaml_fsevent_iter_next,
    .destroy = yaml_fsevent_iter_destroy,
};

static const struct rbh_iterator YAML_FSEVENT_ITERATOR = {
    .ops = &YAML_FSEVENT_ITER_OPS,
};

static const struct rbh_iterator_operations YAML_SOURCE_ITER_OPS = {
    .next = yaml_source_iter_next,
    .destroy = yaml_source_iter_destroy,
};

static const struct source FILE_SOURCE = {
    .name = "file",
    .fsevents = {
        .ops = &YAML_SOURCE_ITER_OPS,
    },
};

struct source *
source_from_file(FILE *file)
{
    struct rbh_fsevent *fsevent;

    initialize_source_stack(sizeof(struct rbh_value_pair) * (1 << 7));
    fsevent = source_stack_alloc(NULL, sizeof(*fsevent));

    return yaml_fsevent_init(file, YAML_FSEVENT_ITERATOR, &FILE_SOURCE,
                             fsevent);
}
