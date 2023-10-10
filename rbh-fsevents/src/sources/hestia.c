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

enum link_field {
    LF_UNKNOWN,
    LF_ID,
    LF_ATTRS,
    LF_TIME,
};

static enum link_field
str2link_field(const char *string)
{
    switch (*string++) {
    case 'i': /* id */
        if (strcmp(string, "d"))
            break;
        return LF_ID;
    case 'a': /* attrs */
        if (strcmp(string, "ttrs"))
            break;
        return LF_ATTRS;
    case 't': /* time */
        if (strcmp(string, "ime"))
            break;
        return LF_TIME;
    }

    errno = EINVAL;
    return LF_UNKNOWN;
}

/* "CREATE" events in Hestia are of the form:
 *
 * ---
 * !create
 * attrs:
 *   dataset.id: "70c88005-aa7e-bce6-7583-0d8b26926c25"
 * id: "421b3153-9108-d1ef-3413-945177dd4ab3"
 * time: 1696837025493616528
 * ...
 *
 */
static bool
parse_create(yaml_parser_t *parser, struct rbh_fsevent *link,
             struct rbh_value_map *map)
{
    struct rbh_id *parent_id;
    bool seen_id = false;

    /* Objects have no parent and no path */
    parent_id = malloc(sizeof(parent_id));
    if (parent_id == NULL)
        error(EXIT_FAILURE, 0, "malloc");

    parent_id->size = 0;
    parent_id->data = NULL;
    link->link.parent_id = parent_id;

    link->xattrs.count = 0;

    while (true) {
        enum link_field field;
        yaml_event_t event;
        const char *key;
        int save_errno;
        bool success;

        if (!yaml_parser_parse(parser, &event))
            parser_error(parser);

        if (event.type == YAML_MAPPING_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }

        if (!yaml_parse_string(&event, &key, NULL)) {
            save_errno = errno;
            yaml_event_delete(&event);
            errno = save_errno;
            return false;
        }

        field = str2link_field(key);
        save_errno = errno;
        yaml_event_delete(&event);

        switch (field) {
        case LF_UNKNOWN:
            errno = save_errno;
            return false;
        case LF_ID:
            seen_id = true;

            success = parse_name(parser, &link->link.name);
            link->id.data = strdup(link->link.name);
            if (!link->id.data)
                error(EXIT_FAILURE, 0, "malloc");

            link->id.size = strlen(link->id.data);
            break;
        case LF_ATTRS:
            /* The attributes are not namespace ones, so we store them for
             * later in another map
             */
            success = parse_rbh_value_map(parser, map);
            break;
        case LF_TIME:
            /* This is the time of the changelog, close but not equal to the
             * object atime/mtime/ctime, so we skip it
             */
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            success = true;
            break;
        }

        if (!success)
            return false;
    }

    return seen_id;
}

enum fsevent_type {
    FT_UNKNOWN,
    FT_CREATE,
};

static enum fsevent_type
str2fsevent_type(const char *string)
{
    if (strcmp(string, "!create") == 0)
        return FT_CREATE;

    errno = EINVAL;
    return FT_UNKNOWN;
}

bool
parse_hestia_event(yaml_parser_t *parser, struct rbh_fsevent *fsevent,
                   struct rbh_value_map *map)
{
    enum fsevent_type type;
    yaml_event_t event;
    const char *tag;
    int save_errno;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    if (event.type != YAML_MAPPING_START_EVENT) {
        yaml_event_delete(&event);
        errno = EINVAL;
        return false;
    }

    tag = yaml_mapping_tag(&event);
    if (tag == NULL) {
        yaml_event_delete(&event);
        errno = EINVAL;
        return false;
    }

    type = str2fsevent_type(tag);
    save_errno = errno;
    yaml_event_delete(&event);

    switch (type) {
    case FT_UNKNOWN:
        errno = save_errno;
        return false;
    case FT_CREATE:
        fsevent->type = RBH_FET_LINK;
        return parse_create(parser, fsevent, map);
    default:
        assert(false);
        __builtin_unreachable();
    }
}
static const void *
hestia_fsevent_iter_next(void *iterator)
{
    struct yaml_fsevent_iterator *fsevents = iterator;
    yaml_event_type_t type;
    yaml_event_t event;

    if (fsevents->exhausted) {
        errno = ENODATA;
        return NULL;
    }

    /* If there are additional xattrs to manage, create a specific event */
    if (fsevents->additional_xattr.count != 0) {
        struct rbh_fsevent *new_inode_event =
            rbh_fsevent_xattr_new(&fsevents->fsevent.id,
                                  &fsevents->additional_xattr);
        fsevents->additional_xattr.count = 0;

        return new_inode_event;
    }

    if (!yaml_parser_parse(&fsevents->parser, &event))
        parser_error(&fsevents->parser);

    type = event.type;
    yaml_event_delete(&event);

    switch (type) {
    case YAML_DOCUMENT_START_EVENT:
        /* Remove any trace of the previous parsed fsevent */
        memset(&fsevents->fsevent, 0, sizeof(fsevents->fsevent));

        if (!parse_hestia_event(&fsevents->parser, &fsevents->fsevent,
                                &fsevents->additional_xattr))
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
hestia_fsevent_iter_destroy(void *iterator)
{
    struct yaml_fsevent_iterator *fsevents = iterator;

    yaml_parser_delete(&fsevents->parser);
}

static const struct rbh_iterator_operations HESTIA_FSEVENT_ITER_OPS = {
    .next = hestia_fsevent_iter_next,
    .destroy = hestia_fsevent_iter_destroy,
};

static const struct rbh_iterator HESTIA_FSEVENT_ITERATOR = {
    .ops = &HESTIA_FSEVENT_ITER_OPS,
};

static const struct rbh_iterator_operations YAML_SOURCE_ITER_OPS = {
    .next = yaml_source_iter_next,
    .destroy = yaml_source_iter_destroy,
};

static const struct source FILE_SOURCE = {
    .name = "hestia",
    .fsevents = {
        .ops = &YAML_SOURCE_ITER_OPS,
    },
};

struct source *
source_from_hestia_file(FILE *file)
{
    struct file_source *source;

    source = malloc(sizeof(*source));
    if (source == NULL)
        error(EXIT_FAILURE, 0, "malloc");

    yaml_fsevent_init(&source->fsevents, file, HESTIA_FSEVENT_ITERATOR);

    source->source = FILE_SOURCE;
    source->file = file;
    return &source->source;
}
