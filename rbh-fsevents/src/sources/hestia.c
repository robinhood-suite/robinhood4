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
#include <robinhood/itertools.h>
#include <robinhood/serialization.h>

#include "source.h"
#include "yaml_file.h"
#include "utils.h"

enum event_fields {
    EF_UNKNOWN,
    EF_ATTRS,
    EF_CTIME,
    EF_ID,
    EF_MTIME,
    EF_SIZE,
    EF_TIERS,
    EF_TIME,
    EF_USER_MD,
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
    case 'a': /* attrs */
        if (strcmp(string, "ttrs"))
            break;
        return EF_ATTRS;
    case 'c': /* ctime */
        if (strcmp(string, "time"))
            break;
        return EF_CTIME;
    case 'i': /* id */
        if (strcmp(string, "d"))
            break;
        return EF_ID;
    case 'm': /* mtime */
        if (strcmp(string, "time"))
            break;
        return EF_MTIME;
    case 's': /* size */
        if (strcmp(string, "ize"))
            break;
        return EF_SIZE;
    case 't':
        if (*string++ != 'i')
            break;

        switch (*string++) {
        case 'e': /* tiers */
            if (strcmp(string, "rs"))
                break;
            return EF_TIERS;
        case 'm': /* timer */
            if (*string != 'e')
                break;
            return EF_TIME;
        }
        break;
    case 'u': /* user_metadata */
        if (strcmp(string, "ser_metadata"))
            break;
        return EF_USER_MD;
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

/* Copy the ID set in new_events[0] to the other events in new_events, according
 * to the number of events given as n_events.
 */
static void
copy_id_in_events(struct rbh_fsevent *new_events, size_t n_events)
{
    for (size_t i = 1; i < n_events; ++i) {
        new_events[i].id.data = new_events[0].id.data;
        new_events[i].id.size = new_events[0].id.size;
    }
}

static bool
parse_attrs(yaml_parser_t *parser, struct rbh_value_map *map,
            struct rbh_statx *statx)
{
    struct rbh_value_pair *pairs;
    bool seen_user_md = false;
    bool seen_tiers = false;
    bool seen_size = false;
    yaml_event_t event;
    int save_errno;

    pairs = source_stack_alloc(NULL, sizeof(*pairs) * 2);
    if (pairs == NULL)
        error(EXIT_FAILURE, errno,
              "source_stack_alloc on pairs in parse_attrs");

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    save_errno = errno;
    yaml_event_delete(&event);
    errno = save_errno;

    while (true) {
        enum key_parse_result next;
        struct rbh_value *value;
        enum event_fields field;
        int64_t event_size;
        bool success;

        next = get_next_key(parser, &event, &field);
        if (next == KPR_END)
            break;
        else if (next == KPR_ERROR)
            return false;

        switch (field) {
        case EF_USER_MD:
            pairs[0].key = source_stack_alloc("user_metadata",
                                              strlen("user_metadata") + 1);
            if (pairs[0].key == NULL)
                error(EXIT_FAILURE, errno,
                      "source_stack_alloc on pairs[0].key in parse_attrs");

            value = source_stack_alloc(NULL, sizeof(*value));
            if (value == NULL)
                error(EXIT_FAILURE, errno,
                      "source_stack_alloc on value in parse_attrs");

            success = parse_rbh_value_map(parser, &value->map, true);
            value->type = RBH_VT_MAP;
            pairs[0].value = value;
            seen_user_md = true;
            break;
        case EF_TIERS:
            pairs[1].key = source_stack_alloc("tiers", strlen("tiers") + 1);
            if (pairs[1].key == NULL)
                error(EXIT_FAILURE, errno,
                      "source_stack_alloc on pairs[1].key in parse_attrs");

            value = source_stack_alloc(NULL, sizeof(*value));
            if (value == NULL)
                error(EXIT_FAILURE, errno,
                      "source_stack_alloc on value in parse_attrs");

            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            success = parse_sequence(parser, value);
            value->type = RBH_VT_SEQUENCE;
            pairs[1].value = value;
            seen_tiers = true;
            break;
        case EF_SIZE:
            seen_size = true;
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            success = parse_int64(&event, &event_size);
            if (!success)
                return false;

            statx->stx_mask |= RBH_STATX_SIZE;
            statx->stx_size = event_size;

            save_errno = errno;
            yaml_event_delete(&event);
            errno = save_errno;
            break;
        default:
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            save_errno = errno;
            yaml_event_delete(&event);
            errno = save_errno;
            success = true;
            break;
        }

        if (!success)
            return false;
    }

    map->pairs = pairs;
    map->count = 2;

    return seen_user_md && seen_tiers && seen_size;
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
parse_read(yaml_parser_t *parser, struct rbh_iterator **fsevents_iterator)
{
    struct rbh_fsevent *new_read_event;
    struct rbh_statx *statx;
    bool seen_time = false;
    bool seen_id = false;
    int64_t event_time;

    new_read_event = source_stack_alloc(NULL, sizeof(*new_read_event));
    if (new_read_event == NULL)
        error(EXIT_FAILURE, errno, "source_stack_alloc in parse_read");

    new_read_event[0].type = RBH_FET_UPSERT;
    new_read_event[0].xattrs.count = 0;
    new_read_event[0].xattrs.pairs = NULL;
    new_read_event[0].upsert.symlink = NULL;

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

            success = parse_name(parser, &new_read_event[0].id.data);
            new_read_event[0].id.size = strlen(new_read_event[0].id.data) + 1;
            break;
        case EF_TIME:
            seen_time = true;
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            success = parse_int64(&event, &event_time);
            if (!success)
                return false;

            statx = source_stack_alloc(NULL, sizeof(*statx));
            if (!statx)
                error(EXIT_FAILURE, errno, "source_stack_alloc in parse_read");

            statx->stx_mask = RBH_STATX_ATIME;
            statx->stx_atime.tv_sec = event_time;
            statx->stx_atime.tv_nsec = 0;

            new_read_event[0].upsert.statx = statx;

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

    *fsevents_iterator = rbh_iter_array(new_read_event, sizeof(*new_read_event),
                                        1, NULL);

    return seen_id && seen_time;
}

static void
initialize_update_fsevents(struct rbh_fsevent *new_update_events)
{
    new_update_events[0].type = RBH_FET_UPSERT;
    new_update_events[0].xattrs.pairs = NULL;
    new_update_events[0].xattrs.count = 0;
    new_update_events[0].upsert.symlink = NULL;

    new_update_events[1].type = RBH_FET_XATTR;
    new_update_events[1].link.parent_id = NULL;
    new_update_events[1].link.name = NULL;
}

static int64_t
parse_update_time(yaml_parser_t *parser)
{
    int64_t event_time;
    yaml_event_t event;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    if (!parse_int64(&event, &event_time))
        error(EXIT_FAILURE, errno,
              "failed to parse time value '%s' in Hestia event",
              yaml_scalar_value(&event));

    yaml_event_delete(&event);
    return event_time;
}

/* "UPDATE" events in Hestia are of the form:
 * ---
 * !update
 * ctime: 1706606568729909
 * mtime: 1706606568697886
 * attrs:
 *   user_metadata:
 *     my_key: "my_value"
 *   tiers:
 *     []
 *   size: 0
 * id: "blob"
 * time: 1706606568729909
 * ...
 *
 */
static bool
parse_update(yaml_parser_t *parser, struct rbh_iterator **fsevents_iterator)
{
    struct rbh_fsevent *new_update_events;
    struct rbh_statx *statx;
    bool seen_attrs = false;
    bool seen_ctime = false;
    bool seen_mtime = false;
    bool seen_id = false;

    new_update_events = source_stack_alloc(NULL,
                                           sizeof(*new_update_events) * 2);
    if (new_update_events == NULL)
        error(EXIT_FAILURE, errno,
              "source_stack_alloc on new_update_events in parse_update");

    initialize_update_fsevents(new_update_events);

    statx = source_stack_alloc(NULL, sizeof(*statx));
    if (!statx)
        error(EXIT_FAILURE, errno,
              "source_stack_alloc on statx in parse_update");

    statx->stx_mask = 0;

    while (true) {
        enum event_fields field = 0;
        enum key_parse_result next;
        int64_t event_time;
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

            success = parse_name(parser, &new_update_events[0].id.data);
            new_update_events[0].id.size =
                strlen(new_update_events[0].id.data) + 1;
            copy_id_in_events(new_update_events, 2);
            break;
        case EF_CTIME:
            event_time = parse_update_time(parser);
            success = true;
            seen_ctime = true;

            statx->stx_mask |= RBH_STATX_CTIME;
            statx->stx_ctime.tv_sec = event_time;
            statx->stx_ctime.tv_nsec = 0;

            break;
        case EF_MTIME:
            event_time = parse_update_time(parser);
            success = true;
            seen_mtime = true;

            /* Do not update the ctime here because the update event contains
             * both ctime and mtime.
             */
            statx->stx_mask |= RBH_STATX_ATIME | RBH_STATX_MTIME;
            statx->stx_atime.tv_sec = event_time;
            statx->stx_atime.tv_nsec = 0;
            statx->stx_mtime.tv_sec = event_time;
            statx->stx_mtime.tv_nsec = 0;

            break;
        case EF_TIME:
            /* The 'time' component of each update event is now equal to the
             * ctime and/or the mtime, so it is redundant and doesn't need to be
             * considered.
             */
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            save_errno = errno;
            yaml_event_delete(&event);
            errno = save_errno;
            success = true;
            break;
        case EF_ATTRS:
            seen_attrs = true;
            success = parse_attrs(parser, &new_update_events[1].xattrs, statx);
            break;
        default:
            return false;
        }

        if (!success)
            return false;
    }

    new_update_events[0].upsert.statx = statx;

    *fsevents_iterator = rbh_iter_array(new_update_events,
                                        sizeof(*new_update_events), 2, NULL);

    return seen_id && seen_attrs && seen_ctime && seen_mtime;
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
parse_remove(yaml_parser_t *parser, struct rbh_iterator **fsevents_iterator)
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
            new_delete_event[0].id.size =
                strlen(new_delete_event[0].id.data) + 1;
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

    *fsevents_iterator = rbh_iter_array(new_delete_event,
                                        sizeof(*new_delete_event), 1, NULL);

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
set_object_id_in_ns_path(const char *name, struct rbh_value_map *xattrs)
{
    struct rbh_value_pair *pair;
    struct rbh_value *value;

    value = source_stack_alloc(NULL, sizeof(*value));
    if (value == NULL)
        error(EXIT_FAILURE, errno,
              "source_stack_alloc on value in set_object_id_in_ns_path");

    value->type = RBH_VT_STRING;
    value->string = source_stack_alloc(name, strlen(name) + 1);
    if (value->string == NULL)
        error(EXIT_FAILURE, errno,
              "source_stack_alloc on value->string in "
              "set_object_id_in_ns_path");

    pair = source_stack_alloc(NULL, sizeof(*pair));
    if (pair == NULL)
        error(EXIT_FAILURE, errno,
              "source_stack_alloc on pair in set_object_id_in_ns_path");

    pair->value = value;
    pair->key = "path";

    xattrs->pairs = pair;
    xattrs->count = 1;
}

/* "CREATE" events in Hestia are of the form:
 *
 * ---
 * !create
 * attrs:
 *   user_metadata:
 *     {}
 *   size: 0
 *   tiers:
 *     []
 * time: 1701418948885961
 * id: "d198c172-35ff-d962-a3db-027cdcf9116c"
 * ...
 *
 */
static bool
parse_create(yaml_parser_t *parser, struct rbh_iterator **fsevents_iterator)
{
    struct rbh_fsevent *new_create_events;
    struct rbh_statx *statx;
    bool seen_id = false;
    int64_t event_time;
    int id_length;

    new_create_events = source_stack_alloc(NULL,
                                           sizeof(*new_create_events) * 3);
    if (new_create_events == NULL)
        error(EXIT_FAILURE, errno,
              "source_stack_alloc on new_create_events in parse_create");

    initialize_create_fsevents(new_create_events);

    statx = source_stack_alloc(NULL, sizeof(*statx));
    if (!statx)
        error(EXIT_FAILURE, errno,
              "source_stack_alloc on statx in parse_create");

    statx->stx_mask = 0;

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
            id_length = strlen(new_create_events[0].link.name) + 1;
            new_create_events[0].id.data = source_stack_alloc(
                new_create_events[0].link.name, id_length
                );

            if (!new_create_events[0].id.data)
                error(EXIT_FAILURE, errno,
                      "source_stack_alloc in parse_create");

            new_create_events[0].id.size = id_length;
            copy_id_in_events(new_create_events, 3);
            set_object_id_in_ns_path(new_create_events[0].link.name,
                                     &new_create_events[0].xattrs);
            break;
        case EF_ATTRS:
            success = parse_attrs(parser, &new_create_events[1].xattrs, statx);
            break;
        case EF_TIME:
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            success = parse_int64(&event, &event_time);
            if (!success)
                error(EXIT_FAILURE, errno, "parse_int64 in parse_create");

            statx->stx_mask |= RBH_STATX_ATIME | RBH_STATX_BTIME |
                               RBH_STATX_CTIME;
            statx->stx_atime.tv_sec = event_time;
            statx->stx_atime.tv_nsec = 0;
            statx->stx_btime.tv_sec = event_time;
            statx->stx_btime.tv_nsec = 0;
            statx->stx_ctime.tv_sec = event_time;
            statx->stx_ctime.tv_nsec = 0;

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

    new_create_events[2].upsert.statx = statx;

    *fsevents_iterator = rbh_iter_array(new_create_events,
                                        sizeof(*new_create_events), 3, NULL);

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
        return parse_create(parser,
                            (struct rbh_iterator **)&fsevents->source_item);
    case FT_REMOVE:
        return parse_remove(parser,
                            (struct rbh_iterator **)&fsevents->source_item);
    case FT_UPDATE:
        return parse_update(parser,
                            (struct rbh_iterator **)&fsevents->source_item);
    case FT_READ:
        return parse_read(parser,
                          (struct rbh_iterator **)&fsevents->source_item);
    default:
        assert(false);
        __builtin_unreachable();
    }
}

static const void *
hestia_fsevent_iter_next(void *iterator)
{
    struct yaml_fsevent_iterator *fsevents = iterator;
    struct rbh_iterator *fsevents_iterator;
    const struct rbh_fsevent *to_return;
    yaml_event_type_t type;
    yaml_event_t event;

    fsevents_iterator = fsevents->source_item;

    if (fsevents->exhausted) {
        if (fsevents_iterator) {
            rbh_iter_destroy(fsevents_iterator);
            fsevents->source_item = NULL;
        }

        errno = ENODATA;
        return NULL;
    }

    if (fsevents_iterator) {
        to_return = rbh_iter_next(fsevents_iterator);
        if (to_return != NULL)
            /* If the fsevent found is not NULL, return it. Otherwise it means
             * we generated all fsevents corresponding to a Hestia event, so
             * parse the next one.
             */
            goto return_fsevent;

        rbh_iter_destroy(fsevents_iterator);
        fsevents->source_item = NULL;
    }

    if (!yaml_parser_parse(&fsevents->parser, &event))
        parser_error(&fsevents->parser);

    type = event.type;
    yaml_event_delete(&event);

    switch (type) {
    case YAML_DOCUMENT_START_EVENT:
        if (!parse_hestia_event(fsevents))
            parser_error(&fsevents->parser);

        if (!yaml_parser_parse(&fsevents->parser, &event))
            parser_error(&fsevents->parser);

        assert(event.type == YAML_DOCUMENT_END_EVENT);
        yaml_event_delete(&event);

        break;
    case YAML_STREAM_END_EVENT:
        fsevents->exhausted = true;
        errno = ENODATA;
        return NULL;
    default:
        error(EXIT_FAILURE, 0, "unexpected YAML event: type = %i", event.type);
        __builtin_unreachable();
    }

    to_return = rbh_iter_next((struct rbh_iterator *)fsevents->source_item);

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

    return yaml_fsevent_init(file, HESTIA_FSEVENT_ITERATOR, &FILE_SOURCE, NULL);
}
