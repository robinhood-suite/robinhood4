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
#include <robinhood/itertools.h>

#include "serialization.h"
#include "source.h"
#include "yaml_file.h"
#include "utils.h"

static __thread struct rbh_iterator *fsevents_iterator;

static void __attribute__((destructor))
free_fsevents_iterator(void)
{
    if (fsevents_iterator)
        rbh_iter_destroy(fsevents_iterator);
}

enum event_fields {
    EF_UNKNOWN,
    EF_ID,
    EF_ATTRS,
    EF_TIME,
};

enum key_parse_result {
    KPR_OK,
    KPR_END,
    KPR_ERROR,
};

static enum event_fields
str2event_fields(const char *string)
{
    switch (*string++) {
    case 'i': /* id */
        if (strcmp(string, "d"))
            break;
        return EF_ID;
    case 'a': /* attrs */
        if (strcmp(string, "ttrs"))
            break;
        return EF_ATTRS;
    case 't': /* time */
        if (strcmp(string, "ime"))
            break;
        return EF_TIME;
    }

    errno = EINVAL;
    return EF_UNKNOWN;
}

static enum key_parse_result
get_next_key(yaml_parser_t *parser, yaml_event_t *event,
             enum event_fields *field)
{
    const char *key;
    int save_errno;

    if (!yaml_parser_parse(parser, event))
        parser_error(parser);

    if (event->type == YAML_MAPPING_END_EVENT) {
        yaml_event_delete(event);
        return KPR_END;
    }

    if (!yaml_parse_string(event, &key, NULL)) {
        *field = -1;

        save_errno = errno;
        yaml_event_delete(event);
        errno = save_errno;
        return KPR_ERROR;
    }

    *field = str2event_fields(key);
    save_errno = errno;
    yaml_event_delete(event);
    errno = save_errno;

    return KPR_OK;
}

static struct rbh_fsevent *
create_new_upsert_fsevent(const struct rbh_id *id,
                          const struct rbh_value_map *xattrs,
                          const struct rbh_statx *statx)
{
    struct rbh_fsevent *upsert = source_stack_alloc(NULL, sizeof(*upsert));

    if (upsert == NULL)
        error(EXIT_FAILURE, errno,
              "source_stack_alloc on upsert in create_new_upsert_fsevent");

    upsert->type = RBH_FET_UPSERT;
    upsert->id.data = source_stack_alloc(id->data, id->size);
    if (upsert->id.data == NULL)
        error(EXIT_FAILURE, errno,
              "source_stack_alloc on id in create_new_upsert_fsevent");

    upsert->id.size = id->size;

    if (xattrs != NULL) {
        upsert->xattrs.pairs = xattrs->pairs;
        upsert->xattrs.count = xattrs->count;
    } else {
        upsert->xattrs.pairs = NULL;
        upsert->xattrs.count = 0;
    }

    if (statx != NULL) {
        upsert->upsert.statx = source_stack_alloc(statx, sizeof(*statx));
    } else {
        upsert->upsert.statx = NULL;
    }

    upsert->upsert.symlink = NULL;

    return upsert;
}

/* "READ" events in Hestia are of the form:
 *
 * ---
 * !read
 * time: 1696837025523562141
 * id: "421b3153-9108-d1ef-3413-945177dd4ab3"
 * ...
 *
 */
static bool
parse_read(yaml_parser_t *parser, struct rbh_fsevent *upsert)
{
    struct rbh_statx *statx;
    bool seen_time = false;
    bool seen_id = false;
    int64_t event_time;

    upsert->xattrs.count = 0;
    upsert->upsert.symlink = NULL;

    while (true) {
        enum event_fields field = 0;
        enum key_parse_result next;
        yaml_event_t event;
        int save_errno;
        bool success;

        next = get_next_key(parser, &event, &field);
        if (next == KPR_END)
            break;
        else if (next == KPR_ERROR)
            return false;

        switch (field) {
        case EF_ID:
            seen_id = true;

            success = parse_name(parser, &upsert->id.data);
            upsert->id.size = strlen(upsert->id.data);
            break;
        case EF_TIME:
            seen_time = true;
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            /* This is the time of the changelog, close but not equal to the
             * object atime/mtime/ctime, but consider it as the object's times
             * for now
             */
            success = parse_int64(&event, &event_time);
            if (!success)
                return false;

            statx = source_stack_alloc(NULL, sizeof(*statx));
            if (!statx)
                error(EXIT_FAILURE, errno, "source_stack_alloc in parse_read");

            statx->stx_mask = RBH_STATX_ATIME;
            statx->stx_atime.tv_sec = event_time;
            statx->stx_atime.tv_nsec = 0;

            upsert->upsert.statx = statx;
            save_errno = errno;
            yaml_event_delete(&event);
            errno = save_errno;
            break;
        default:
            return false;
        }

        if (!success)
            return false;
    }

    return seen_id && seen_time;
}

/* "UPDATE" events in Hestia are of the form:
 *
 * ---
 * !update
 * attrs:
 *   user_metadata.key0: "value0"
 *   user_metadata.key1: "value1"
 * id: "421b3153-9108-d1ef-3413-945177dd4ab3"
 * time: 1696837025494619488
 * ...
 *
 */
static bool
parse_update(yaml_parser_t *parser, struct rbh_fsevent *inode,
             struct rbh_statx *statx)
{
    bool seen_attrs = false;
    bool seen_id = false;
    int64_t event_time;

    inode->link.parent_id = NULL;
    inode->link.name = NULL;

    while (true) {
        enum event_fields field = 0;
        enum key_parse_result next;
        yaml_event_t event;
        int save_errno;
        bool success;

        next = get_next_key(parser, &event, &field);
        if (next == KPR_END)
            break;
        else if (next == KPR_ERROR)
            return false;

        switch (field) {
        case EF_ID:
            seen_id = true;

            success = parse_name(parser, &inode->id.data);
            inode->id.size = strlen(inode->id.data);
            break;
        case EF_TIME:
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            /* This is the time of the changelog, close but not equal to the
             * object atime/mtime/ctime, but consider it as the object's times
             * for now
             */
            success = parse_int64(&event, &event_time);
            if (!success)
                error(EXIT_FAILURE, errno, "parse_int64 in parse_update");

            statx->stx_mask = RBH_STATX_CTIME;
            statx->stx_ctime.tv_sec = event_time;
            statx->stx_ctime.tv_nsec = 0;

            save_errno = errno;
            yaml_event_delete(&event);
            errno = save_errno;
            break;
        case EF_ATTRS:
            seen_attrs = true;
            success = parse_rbh_value_map(parser, &inode->xattrs, true);
            break;
        default:
            return false;
        }

        if (!success)
            return false;
    }

    return seen_id && seen_attrs;
}

/* "REMOVE" events in Hestia are of the form:
 *
 * ---
 * !remove
 * id: "421b3153-9108-d1ef-3413-945177dd4ab3"
 * time: 1696837025493616528
 * ...
 *
 */
static bool
parse_remove(yaml_parser_t *parser)
{
    struct rbh_fsevent *new_delete_event;
    bool seen_id = false;

    new_delete_event = source_stack_alloc(NULL, sizeof(*new_delete_event));
    if (new_delete_event == NULL)
        error(EXIT_FAILURE, errno, "source_stack_alloc in parse_remove");

    new_delete_event[0].type = RBH_FET_DELETE;

    while (true) {
        enum event_fields field = 0;
        enum key_parse_result next;
        yaml_event_t event;
        int save_errno;
        bool success;

        next = get_next_key(parser, &event, &field);
        if (next == KPR_END)
            break;
        else if (next == KPR_ERROR)
            return false;

        switch (field) {
        case EF_ID:
            seen_id = true;

            success = parse_name(parser, &new_delete_event[0].id.data);
            new_delete_event[0].id.size = strlen(new_delete_event[0].id.data);
            break;
        case EF_TIME:
            /* Ignore this part of the event */
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            save_errno = errno;
            yaml_event_delete(&event);
            errno = save_errno;
            success = true;
            break;
        default:
            return false;
        }

        if (!success)
            return false;
    }

    fsevents_iterator = rbh_iter_array(new_delete_event,
                                       sizeof(*new_delete_event), 1);

    return seen_id;
}

static void
initialize_create_fsevents(struct rbh_fsevent *new_create_events)
{
    struct rbh_id *parent_id;

    new_create_events[0].type = RBH_FET_LINK;

    /* Objects have no parent and no path */
    parent_id = source_stack_alloc(NULL, sizeof(*parent_id));
    if (parent_id == NULL)
        error(EXIT_FAILURE, errno, "source_stack_alloc in parse_create");

    parent_id->size = 0;
    parent_id->data = NULL;
    new_create_events[0].link.parent_id = parent_id;
    new_create_events[0].xattrs.count = 0;

    new_create_events[1].type = RBH_FET_XATTR;
    new_create_events[1].ns.parent_id = NULL;
    new_create_events[1].ns.name = NULL;

    new_create_events[2].type = RBH_FET_UPSERT;
    new_create_events[2].xattrs.pairs = NULL;
    new_create_events[2].xattrs.count = 0;
    new_create_events[2].upsert.symlink = NULL;
}

static void
copy_id_in_create_events(struct rbh_fsevent *new_create_events)
{
    new_create_events[1].id.data = new_create_events[0].id.data;
    new_create_events[1].id.size = new_create_events[0].id.size;

    new_create_events[2].id.data = new_create_events[0].id.data;
    new_create_events[2].id.size = new_create_events[0].id.size;
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
parse_create(yaml_parser_t *parser)
{
    struct rbh_fsevent *new_create_events;
    struct rbh_statx *statx;
    bool seen_id = false;
    int64_t event_time;
    int id_length;

    new_create_events = source_stack_alloc(NULL,
                                           sizeof(*new_create_events) * 3);
    if (new_create_events == NULL)
        error(EXIT_FAILURE, errno, "source_stack_alloc in parse_create");

    initialize_create_fsevents(new_create_events);

    while (true) {
        enum event_fields field = 0;
        enum key_parse_result next;
        yaml_event_t event;
        int save_errno;
        bool success;

        next = get_next_key(parser, &event, &field);
        if (next == KPR_END)
            break;
        else if (next == KPR_ERROR)
            return false;

        switch (field) {
        case EF_UNKNOWN:
            return false;
        case EF_ID:
            seen_id = true;

            success = parse_name(parser, &new_create_events[0].link.name);
            id_length = strlen(new_create_events[0].link.name);
            new_create_events[0].id.data = source_stack_alloc(
                new_create_events[0].link.name, id_length
                );

            if (!new_create_events[0].id.data)
                error(EXIT_FAILURE, errno,
                      "source_stack_alloc in parse_create");

            new_create_events[0].id.size = id_length;
            copy_id_in_create_events(new_create_events);
            break;
        case EF_ATTRS:
            /* The attributes are not namespace ones, so we store them for
             * later in another map
             */
            success = parse_rbh_value_map(parser, &new_create_events[1].xattrs,
                                          true);
            break;
        case EF_TIME:
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            success = parse_int64(&event, &event_time);
            if (!success)
                error(EXIT_FAILURE, errno, "parse_int64 in parse_create");

            statx = source_stack_alloc(NULL, sizeof(*statx));
            if (!statx)
                error(EXIT_FAILURE, errno,
                      "source_stack_alloc in parse_create");

            statx->stx_mask = RBH_STATX_ATIME | RBH_STATX_BTIME |
                              RBH_STATX_CTIME | RBH_STATX_MTIME;
            statx->stx_atime.tv_sec = event_time;
            statx->stx_atime.tv_nsec = 0;
            statx->stx_btime.tv_sec = event_time;
            statx->stx_btime.tv_nsec = 0;
            statx->stx_ctime.tv_sec = event_time;
            statx->stx_ctime.tv_nsec = 0;
            statx->stx_mtime.tv_sec = event_time;
            statx->stx_mtime.tv_nsec = 0;

            new_create_events[2].upsert.statx = statx;

            save_errno = errno;
            yaml_event_delete(&event);
            errno = save_errno;
            break;
        default:
            return false;
        }

        if (!success)
            return false;
    }

    fsevents_iterator = rbh_iter_array(new_create_events,
                                       sizeof(*new_create_events), 3);

    return seen_id;
}

enum fsevent_type {
    FT_UNKNOWN,
    FT_CREATE,
    FT_REMOVE,
    FT_UPDATE,
    FT_READ,
};

static enum fsevent_type
str2fsevent_type(const char *string)
{
    if (strcmp(string, "!create") == 0)
        return FT_CREATE;
    if (strcmp(string, "!remove") == 0)
        return FT_REMOVE;
    if (strcmp(string, "!update") == 0)
        return FT_UPDATE;
    if (strcmp(string, "!read") == 0)
        return FT_READ;

    errno = EINVAL;
    return FT_UNKNOWN;
}

bool
parse_hestia_event(struct yaml_fsevent_iterator *fsevents)
{
    struct rbh_statx *statx = &fsevents->additional_statx;
    struct rbh_fsevent *fsevent = &fsevents->fsevent;
    yaml_parser_t *parser = &fsevents->parser;
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
        fsevents->enrich_required = true;
        return parse_create(parser);
    case FT_REMOVE:
        fsevent->type = RBH_FET_DELETE;
        return parse_remove(parser);
    case FT_UPDATE:
        fsevent->type = RBH_FET_XATTR;
        fsevents->enrich_required = true;
        return parse_update(parser, fsevent, statx);
    case FT_READ:
        fsevent->type = RBH_FET_UPSERT;
        return parse_read(parser, fsevent);
    default:
        assert(false);
        __builtin_unreachable();
    }
}

static const void *
hestia_fsevent_iter_next(void *iterator)
{
    struct yaml_fsevent_iterator *fsevents = iterator;
    const struct rbh_fsevent *to_return;
    yaml_event_type_t type;
    yaml_event_t event;

    if (fsevents->exhausted) {
        errno = ENODATA;
        return NULL;
    }

    if (fsevents_iterator) {
        to_return = rbh_iter_next(fsevents_iterator);
        if (to_return != NULL)
            goto return_fsevent;

        rbh_iter_destroy(fsevents_iterator);
        fsevents_iterator = NULL;
    }

    if (fsevents->additional_statx.stx_mask != 0) {
        struct rbh_fsevent *new_upsert_event =
            create_new_upsert_fsevent(&fsevents->fsevent.id, NULL,
                                      &fsevents->additional_statx);
        fsevents->additional_statx.stx_mask = 0;

        fsevents_iterator = rbh_iter_array(new_upsert_event,
                                           sizeof(*new_upsert_event), 1);
    } else if (fsevents->enrich_required) {
        struct rbh_value_map map = build_enrich_map(build_empty_map, "hestia");
        struct rbh_fsevent *new_upsert_event;

        if (map.pairs == NULL)
            error(EXIT_FAILURE, errno,
                  "build_enrich_map in hestia_fsevent_iter_next");

        new_upsert_event = create_new_upsert_fsevent(&fsevents->fsevent.id,
                                                     &map, NULL);

        fsevents->enrich_required = false;
        fsevents_iterator = rbh_iter_array(new_upsert_event,
                                           sizeof(*new_upsert_event), 1);
    }

    if (fsevents_iterator) {
        to_return = rbh_iter_next(fsevents_iterator);
        if (to_return != NULL)
            goto return_fsevent;

        rbh_iter_destroy(fsevents_iterator);
        fsevents_iterator = NULL;
    }

    if (!yaml_parser_parse(&fsevents->parser, &event))
        parser_error(&fsevents->parser);

    type = event.type;
    yaml_event_delete(&event);

    switch (type) {
    case YAML_DOCUMENT_START_EVENT:
        /* Remove any trace of the previous parsed fsevent */
        memset(&fsevents->fsevent, 0, sizeof(fsevents->fsevent));

        if (!parse_hestia_event(fsevents))
            parser_error(&fsevents->parser);

        if (!yaml_parser_parse(&fsevents->parser, &event))
            parser_error(&fsevents->parser);

        assert(event.type == YAML_DOCUMENT_END_EVENT);
        yaml_event_delete(&event);

        if (fsevents_iterator != NULL)
            break;

        return &fsevents->fsevent;
    case YAML_STREAM_END_EVENT:
        fsevents->exhausted = true;
        errno = ENODATA;
        return NULL;
    default:
        error(EXIT_FAILURE, 0, "unexpected YAML event: type = %i", event.type);
        __builtin_unreachable();
    }

    to_return = rbh_iter_next(fsevents_iterator);

return_fsevent:
    return to_return;
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
    initialize_source_stack(sizeof(struct rbh_value_pair) * (1 << 7));

    return yaml_fsevent_init(file, HESTIA_FSEVENT_ITERATOR, &FILE_SOURCE);
}
