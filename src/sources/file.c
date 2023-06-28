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

struct yaml_fsevent_iterator {
    struct rbh_iterator iterator;

    struct rbh_fsevent fsevent;
    yaml_parser_t parser;
    bool exhausted;
};

static void __attribute__((noreturn))
parser_error(yaml_parser_t *parser)
{
    error(EXIT_FAILURE, 0, "parser error: %s", parser->problem);
    __builtin_unreachable();
}

static const void *
yaml_fsevent_iter_next(void *iterator)
{
    struct yaml_fsevent_iterator *fsevents = iterator;
    yaml_event_type_t type;
    yaml_event_t event;

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
        memset(&fsevents->fsevent, 0, sizeof(fsevents->fsevent));

        if (!parse_fsevent(&fsevents->parser, &fsevents->fsevent))
            parser_error(&fsevents->parser);

        if (!yaml_parser_parse(&fsevents->parser, &event))
            parser_error(&fsevents->parser);

        assert(event.type == YAML_DOCUMENT_END_EVENT);
        yaml_event_delete(&event);
        return &fsevents->fsevent;
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

static void
yaml_fsevent_init(struct yaml_fsevent_iterator *fsevents, FILE *file)
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

    fsevents->iterator = YAML_FSEVENT_ITERATOR;
    fsevents->exhausted = false;
    fsevents->fsevent.type = 0;
}

struct file_source {
    struct source source;

    struct yaml_fsevent_iterator fsevents;
    FILE *file;
};

static const void *
source_iter_next(void *iterator)
{
    struct file_source *source = iterator;

    return rbh_iter_next(&source->fsevents.iterator);
}

static void
source_iter_destroy(void *iterator)
{
    struct file_source *source = iterator;

    rbh_iter_destroy(&source->fsevents.iterator);
    /* Ignore errors on close */
    fclose(source->file);
    free(source);
}

static const struct rbh_iterator_operations SOURCE_ITER_OPS = {
    .next = source_iter_next,
    .destroy = source_iter_destroy,
};

static const struct source FILE_SOURCE = {
    .name = "file",
    .fsevents = {
        .ops = &SOURCE_ITER_OPS,
    },
};

struct source *
source_from_file(FILE *file)
{
    struct file_source *source;

    source = malloc(sizeof(*source));
    if (source == NULL)
        error(EXIT_FAILURE, 0, "malloc");

    yaml_fsevent_init(&source->fsevents, file);

    source->source = FILE_SOURCE;
    source->file = file;
    return &source->source;
}
