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

#include "serialization.h"
#include "source.h"
#include "yaml_file.h"
#include "utils.h"

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
parse_remove(yaml_parser_t *parser, struct rbh_fsevent *delete)
{
    bool seen_id = false;

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

            success = parse_name(parser, &delete->id.data);
            delete->id.size = strlen(delete->id.data);
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

    return seen_id;
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
             struct rbh_value_map *saved_xattr, struct rbh_statx *statx)
{
    struct rbh_id *parent_id;
    bool seen_id = false;
    int64_t event_time;

    /* Objects have no parent and no path */
    parent_id = source_stack_alloc(NULL, sizeof(*parent_id));
    if (parent_id == NULL)
        error(EXIT_FAILURE, errno, "source_stack_alloc in parse_create");

    parent_id->size = 0;
    parent_id->data = NULL;
    link->link.parent_id = parent_id;

    link->xattrs.count = 0;

    while (true) {
        enum event_fields field = 0;
        enum key_parse_result next;
        yaml_event_t event;
        int save_errno;
        bool success;

        next = get_next_key(parser, &event, &field);
        if (next == KPR_END) {
            break;
        } else if (next == KPR_ERROR) {
            save_errno = errno;
            free(parent_id);
            errno = save_errno;
            return false;
        }

        switch (field) {
        case EF_UNKNOWN:
            free(parent_id);
            errno = save_errno;
            return false;
        case EF_ID:
            seen_id = true;

            success = parse_name(parser, &link->link.name);
            link->id.data = source_stack_alloc(link->link.name,
                                               strlen(link->link.name));
            if (!link->id.data)
                error(EXIT_FAILURE, errno,
                      "source_stack_alloc in parse_create");

            link->id.size = strlen(link->id.data);
            break;
        case EF_ATTRS:
            /* The attributes are not namespace ones, so we store them for
             * later in another map
             */
            success = parse_rbh_value_map(parser, saved_xattr, true);
            break;
        case EF_TIME:
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            success = parse_int64(&event, &event_time);
            if (!success)
                error(EXIT_FAILURE, errno, "parse_int64 in parse_create");

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

            save_errno = errno;
            yaml_event_delete(&event);
            errno = save_errno;
            break;
        default:
            return false;
        }

        if (!success) {
            free(parent_id);
            return false;
        }
    }

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
    struct rbh_value_map *map = &fsevents->additional_xattr;
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
        return parse_create(parser, fsevent, map, statx);
    case FT_REMOVE:
        fsevent->type = RBH_FET_DELETE;
        return parse_remove(parser, fsevent);
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

static struct rbh_fsevent *
create_new_xattr_fsevent(const struct rbh_id *id,
                         const struct rbh_value_map *xattrs)
{
    struct rbh_fsevent *xattr = source_stack_alloc(NULL, sizeof(*xattr));

    if (xattr == NULL)
        error(EXIT_FAILURE, errno,
              "source_stack_alloc on xattr in create_new_xattr_fsevent");

    xattr->type = RBH_FET_XATTR;
    xattr->id.data = source_stack_alloc(id->data, id->size);
    if (xattr->id.data == NULL)
        error(EXIT_FAILURE, errno,
              "source_stack_alloc on id in create_new_xattr_fsevent");

    xattr->id.size = id->size;

    xattr->xattrs.pairs = xattrs->pairs;
    xattr->xattrs.count = xattrs->count;

    xattr->ns.parent_id = NULL;
    xattr->ns.name = NULL;

    return xattr;
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
            create_new_xattr_fsevent(&fsevents->fsevent.id,
                                     &fsevents->additional_xattr);
        fsevents->additional_xattr.count = 0;

        return new_inode_event;
    }

    if (fsevents->additional_statx.stx_mask != 0) {
        struct rbh_fsevent *new_upsert_event =
            create_new_upsert_fsevent(&fsevents->fsevent.id, NULL,
                                      &fsevents->additional_statx);
        fsevents->additional_statx.stx_mask = 0;

        return new_upsert_event;
    }

    if (fsevents->enrich_required) {
        struct rbh_value_map map = build_enrich_map(build_empty_map, "hestia");
        struct rbh_fsevent *new_upsert_event;

        if (map.pairs == NULL)
            error(EXIT_FAILURE, errno,
                  "build_enrich_map in hestia_fsevent_iter_next");

        new_upsert_event = create_new_upsert_fsevent(&fsevents->fsevent.id,
                                                     &map, NULL);

        fsevents->enrich_required = false;
        return new_upsert_event;
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
    initialize_source_stack(sizeof(struct rbh_value_pair) * (1 << 7));

    return yaml_fsevent_init(file, HESTIA_FSEVENT_ITERATOR, &FILE_SOURCE);
}
