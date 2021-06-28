/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <error.h>
#include <stdlib.h>

#include <sys/stat.h>
#ifndef HAVE_STATX
# include <robinhood/statx.h>
#endif

#include <miniyaml.h>
#include <robinhood.h>

#include "serialization.h"

static void __attribute__((noreturn))
parser_error(yaml_parser_t *parser)
{
    error(EXIT_FAILURE, 0, "parser error: %s", parser->problem);
    __builtin_unreachable();
}

static struct {
    struct rbh_sstack *events;
    struct rbh_sstack *pointers;
    struct rbh_sstack *values;
} context;

static void __attribute__((constructor))
context_init(void)
{
    context.events = rbh_sstack_new(sizeof(yaml_event_t) * 64);
    if (context.events == NULL)
        error(EXIT_FAILURE, errno, "rbh_sstack_new");

    context.pointers = rbh_sstack_new(sizeof(void *) * 8);
    if (context.pointers == NULL)
        error(EXIT_FAILURE, errno, "rbh_sstack_new");

    context.values = rbh_sstack_new(sizeof(struct rbh_value) * 64);
    if (context.values == NULL)
        error(EXIT_FAILURE, errno, "rbh_sstack_new");
}

static void
events_flush(void)
{
    while (true) {
        yaml_event_t *events;
        size_t readable;

        events = rbh_sstack_peek(context.events, &readable);
        if (readable == 0)
            break;
        assert(readable % sizeof(*events) == 0);

        for (size_t i = 0; i < readable / sizeof(*events); i++)
            yaml_event_delete(&events[i]);

        rbh_sstack_pop(context.events, readable);
    }
    rbh_sstack_shrink(context.events);
}

static void
pointers_flush(void)
{
    while (true) {
        size_t readable;
        void **pointers;

        pointers = rbh_sstack_peek(context.pointers, &readable);
        if (readable == 0)
            break;
        assert(readable % sizeof(*pointers) == 0);

        for (size_t i = 0; i < readable / sizeof(*pointers); i++)
            free(pointers[i]);

        rbh_sstack_pop(context.pointers, readable);
    }
    rbh_sstack_shrink(context.pointers);
}

static void
values_flush(void)
{
    while (true) {
        size_t readable;

        rbh_sstack_peek(context.values, &readable);
        if (readable == 0)
            break;

        rbh_sstack_pop(context.values, readable);
    }
    rbh_sstack_shrink(context.values);
}

static void
context_reinit(void)
{
    values_flush();
    pointers_flush();
    events_flush();
}

static void __attribute__((destructor))
context_exit(void)
{
    if (context.values) {
        values_flush();
        rbh_sstack_destroy(context.values);
    }
    if (context.pointers) {
        pointers_flush();
        rbh_sstack_destroy(context.pointers);
    }
    if (context.events) {
        events_flush();
        rbh_sstack_destroy(context.events);
    }
}

/*----------------------------------------------------------------------------*
 |                               sized integers                               |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                              uint64_t                              |
     *--------------------------------------------------------------------*/

#define UINT64_TAG "!uint64"

static bool
emit_uint64(yaml_emitter_t *emitter, uint64_t u)
{
    char buffer[sizeof(u) * 4];
    int n;

    n = snprintf(buffer, sizeof(buffer), "%" PRIu64, u);
    return yaml_emit_scalar(emitter, UINT64_TAG, buffer, n,
                            YAML_PLAIN_SCALAR_STYLE);
}

static bool
parse_uint64(const yaml_event_t *event, uint64_t *u)
{
    const char *value = yaml_scalar_value(event);
    const char *tag = yaml_scalar_tag(event);
    uintmax_t umax;
    int save_errno;
    char *end;

    assert(event->type == YAML_SCALAR_EVENT);

    if (tag ? strcmp(tag, UINT64_TAG) : !yaml_scalar_is_plain(event)) {
        errno = EINVAL;
        return false;
    }

    save_errno = errno;
    errno = 0;
    umax = strtoumax(value, &end, 0);
    if ((umax == UINTMAX_MAX) && errno == ERANGE)
        return false;
    if (umax > UINT64_MAX) {
        errno = ERANGE;
        return false;
    }
    if (*end != '\0') {
        errno = EINVAL;
        return false;
    }

    *u = umax;
    errno = save_errno;
    return true;
}

    /*--------------------------------------------------------------------*
     |                              uint32_t                              |
     *--------------------------------------------------------------------*/

#define UINT32_TAG "!uint32"

static bool
emit_uint32(yaml_emitter_t *emitter, uint32_t u)
{
    char buffer[sizeof(u) * 4];
    int n;

    n = snprintf(buffer, sizeof(buffer), "%" PRIu32, u);
    return yaml_emit_scalar(emitter, UINT32_TAG, buffer, n,
                            YAML_PLAIN_SCALAR_STYLE);
}

static bool
parse_uint32(const yaml_event_t *event, uint32_t *u)
{
    const char *value = yaml_scalar_value(event);
    const char *tag = yaml_scalar_tag(event);
    uintmax_t umax;
    int save_errno;
    char *end;

    assert(event->type == YAML_SCALAR_EVENT);

    if (tag ? strcmp(tag, UINT32_TAG) : !yaml_scalar_is_plain(event)) {
        errno = EINVAL;
        return false;
    }

    save_errno = errno;
    errno = 0;
    umax = strtoumax(value, &end, 0);
    if ((umax == UINTMAX_MAX) && errno == ERANGE)
        return false;
    if (umax > UINT32_MAX) {
        errno = ERANGE;
        return false;
    }
    if (*end != '\0') {
        errno = EINVAL;
        return false;
    }

    *u = umax;
    errno = save_errno;
    return true;
}

    /*--------------------------------------------------------------------*
     |                              int64_t                               |
     *--------------------------------------------------------------------*/

#define INT64_TAG "!int64"

static bool
emit_int64(yaml_emitter_t *emitter, int64_t i)
{
    char buffer[sizeof(i) * 4];
    int n;

    n = snprintf(buffer, sizeof(buffer), "%" PRIi64, i);
    return yaml_emit_scalar(emitter, INT64_TAG, buffer, n,
                            YAML_PLAIN_SCALAR_STYLE);
}

static bool
parse_int64(const yaml_event_t *event, int64_t *i)
{
    const char *value = yaml_scalar_value(event);
    const char *tag = yaml_scalar_tag(event);
    intmax_t imax;
    int save_errno;
    char *end;

    assert(event->type == YAML_SCALAR_EVENT);

    if (tag ? strcmp(tag, INT64_TAG) : !yaml_scalar_is_plain(event)) {
        errno = EINVAL;
        return false;
    }

    save_errno = errno;
    errno = 0;
    imax = strtoimax(value, &end, 0);
    if ((imax == INTMAX_MAX || imax == INTMAX_MIN) && errno == ERANGE)
        return false;
    if (imax < INT64_MIN || imax > INT64_MAX) {
        errno = ERANGE;
        return false;
    }
    if (*end != '\0') {
        errno = EINVAL;
        return false;
    }

    *i = imax;
    errno = save_errno;
    return true;
}

    /*--------------------------------------------------------------------*
     |                              int32_t                               |
     *--------------------------------------------------------------------*/

#define INT32_TAG "!int32"

static bool
emit_int32(yaml_emitter_t *emitter, int32_t i)
{
    char buffer[sizeof(i) * 4];
    int n;

    n = snprintf(buffer, sizeof(buffer), "%" PRIi32, i);
    return yaml_emit_scalar(emitter, INT32_TAG, buffer, n,
                            YAML_PLAIN_SCALAR_STYLE);
}

static bool
parse_int32(const yaml_event_t *event, int32_t *i)
{
    const char *value = yaml_scalar_value(event);
    const char *tag = yaml_scalar_tag(event);
    intmax_t imax;
    int save_errno;
    char *end;

    assert(event->type == YAML_SCALAR_EVENT);

    if (tag ? strcmp(tag, INT32_TAG) : !yaml_scalar_is_plain(event)) {
        errno = EINVAL;
        return false;
    }

    save_errno = errno;
    errno = 0;
    imax = strtoimax(value, &end, 0);
    if ((imax == INTMAX_MAX || imax == INTMAX_MIN) && errno == ERANGE)
        return false;
    if (imax < INT32_MIN || imax > INT32_MAX) {
        errno = ERANGE;
        return false;
    }
    if (*end != '\0') {
        errno = EINVAL;
        return false;
    }

    *i = imax;
    errno = save_errno;
    return true;
}

/*----------------------------------------------------------------------------*
 |                                     id                                     |
 *----------------------------------------------------------------------------*/

static bool
parse_id(yaml_parser_t *parser, struct rbh_id *id)
{
    yaml_event_t event;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    if (event.type != YAML_SCALAR_EVENT) {
        yaml_event_delete(&event);
        errno = EINVAL;
        return false;
    }

    /* FIXME: this relies on libyaml's internals, it is a hack */
    id->data = yaml_scalar_value(&event);
    if (!yaml_parse_binary(&event, (char *)event.data.scalar.value,
                           &id->size)) {
        int save_errno = errno;

        yaml_event_delete(&event);
        errno = save_errno;
        return false;
    }

    if (rbh_sstack_push(context.events, &event, sizeof(event)) == NULL) {
        int save_errno = errno;

        yaml_event_delete(&event);
        errno = save_errno;
        return false;
    }

    return true;
}

/*----------------------------------------------------------------------------*
 |                                   value                                    |
 *----------------------------------------------------------------------------*/

static bool
emit_rbh_value(yaml_emitter_t *emitter, const struct rbh_value *value);

static bool
parse_rbh_value(yaml_parser_t *parser, yaml_event_t *event,
                struct rbh_value *value);

    /*--------------------------------------------------------------------*
     |                                map                                 |
     *--------------------------------------------------------------------*/

static bool
emit_rbh_value_map(yaml_emitter_t *emitter, const struct rbh_value_map *map)
{
    if (!yaml_emit_mapping_start(emitter, NULL))
        return false;

    for (size_t i = 0; i < map->count; i++) {
        const struct rbh_value_pair *pair = &map->pairs[i];

        if (!YAML_EMIT_STRING(emitter, pair->key))
            return false;

        if (!emit_rbh_value(emitter, pair->value))
            return false;
    }

    return yaml_emit_mapping_end(emitter);
}

/* This function takes the ownership of `event' */
static bool
parse_rbh_value_pair(yaml_parser_t *parser, yaml_event_t *event,
                     struct rbh_value_pair *pair)
{
    struct rbh_value *value;

    /* Parse the key */
    if (!yaml_parse_string(event, &pair->key, NULL)) {
        yaml_event_delete(event);
        return false;
    }

    /* Store the event that references the key */
    if (rbh_sstack_push(context.events, event, sizeof(*event)) == NULL) {
        yaml_event_delete(event);
        return false;
    }

    /* Parse the value */
    if (!yaml_parser_parse(parser, event))
        parser_error(parser);

    /* It could be a NULL... */
    if (event->type == YAML_SCALAR_EVENT && yaml_parse_null(event)) {
        yaml_event_delete(event);
        pair->value = NULL;
        return true;
    }

    /* ... Or it could just be a regular value */

    /* Allocate a struct rbh_value on a context stack */
    value = rbh_sstack_push(context.values, NULL, sizeof(*value));
    if (value == NULL)
        return false;
    pair->value = value;

    if (!parse_rbh_value(parser, event, value)) {
        int save_errno = errno;

        rbh_sstack_pop(context.values, sizeof(*value));
        errno = save_errno;
        return false;
    }

    return true;
}

static bool
parse_rbh_value_map(yaml_parser_t *parser, struct rbh_value_map *map)
{
    struct rbh_value_pair *pairs;
    size_t count = 1; /* TODO: fine tune this */
    size_t i = 0;
    bool end = false;

    pairs = malloc(sizeof(*pairs) * count);
    if (pairs == NULL)
        return false;

    do {
        yaml_event_t event;

        if (!yaml_parser_parse(parser, &event))
            parser_error(parser);

        /* Every key must be a scalar */
        switch (event.type) {
        case YAML_SCALAR_EVENT:
            break;
        case YAML_MAPPING_END_EVENT:
            yaml_event_delete(&event);
            end = true;
            continue;
        default:
            yaml_event_delete(&event);
            free(pairs);
            errno = EINVAL;
            return false;
        }

        if (i == count) {
            void *tmp = pairs;

            count *= 2;
            tmp = reallocarray(tmp, count, sizeof(*pairs));
            if (tmp == NULL) {
                int save_errno = errno;

                yaml_event_delete(&event);
                free(pairs);
                errno = save_errno;
                return false;
            }
            pairs = tmp;
        }

        if (!parse_rbh_value_pair(parser, &event, &pairs[i++])) {
            int save_errno = errno;

            yaml_event_delete(&event);
            free(pairs);
            errno = save_errno;
            return false;
        }
    } while (!end);

    if (rbh_sstack_push(context.pointers, &pairs, sizeof(pairs)) == NULL) {
        int save_errno = errno;

        free(pairs);
        errno = save_errno;
        return false;
    }

    map->pairs = pairs;
    map->count = i;

    return true;
}

    /*--------------------------------------------------------------------*
     |                              sequence                              |
     *--------------------------------------------------------------------*/

static bool
parse_sequence(yaml_parser_t *parser, struct rbh_value *sequence)
{
    struct rbh_value *values;
    size_t count = 1; /* TODO: fine tune this */
    size_t i = 0;

    values = malloc(sizeof(*values) * count);
    if (values == NULL)
        return false;

    while (true) {
        yaml_event_t event;

        if (!yaml_parser_parse(parser, &event))
            parser_error(parser);

        if (event.type == YAML_SEQUENCE_END_EVENT) {
            yaml_event_delete(&event);
            break;
        }

        if (i == count) {
            void *tmp = values;

            count *= 2;
            tmp = reallocarray(tmp, count, sizeof(*values));
            if (tmp == NULL) {
                int save_errno = errno;

                yaml_event_delete(&event);
                free(values);
                errno = save_errno;
                return false;
            }
            values = tmp;
        }

        if (!parse_rbh_value(parser, &event, &values[i++])) {
            int save_errno = errno;

            free(values);
            errno = save_errno;
            return false;
        }
    }

    if (rbh_sstack_push(context.pointers, &values, sizeof(values)) == NULL) {
        int save_errno = errno;

        free(values);
        errno = save_errno;
        return false;
    }

    sequence->sequence.values = values;
    sequence->sequence.count = i;

    return true;
}

    /*--------------------------------------------------------------------*
     |                               regex                                |
     *--------------------------------------------------------------------*/

#define REGEX_TAG "!regex"

static bool
emit_regex(yaml_emitter_t *emitter, const char *regex, unsigned int options)
{
    return yaml_emit_mapping_start(emitter, REGEX_TAG)
        && YAML_EMIT_STRING(emitter, "regex")
        && YAML_EMIT_STRING(emitter, regex)
        && YAML_EMIT_STRING(emitter, "options")
        && yaml_emit_integer(emitter, options)
        && yaml_emit_mapping_end(emitter);
}

static bool
parse_regex_mapping(yaml_parser_t *parser __attribute__((unused)),
                    const char **regex __attribute__((unused)),
                    unsigned int *options __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

    /*--------------------------------------------------------------------*
     |                                type                                |
     *--------------------------------------------------------------------*/

#ifndef YAML_BINARY_TAG
# define YAML_BINARY_TAG "tag:yaml.org,2002:binary"
#endif

static bool
parse_value_type(const yaml_event_t *event, enum rbh_value_type *type)
{
    const char *tag;

    switch (event->type) {
    case YAML_MAPPING_START_EVENT:
        /* Map or regex? */
        tag = yaml_mapping_tag(event);
        if (tag == NULL) {
            *type = RBH_VT_MAP;
            return true;
        }
        if (strcmp(tag, REGEX_TAG) == 0) {
            *type = RBH_VT_REGEX;
            return true;
        }
        errno = EINVAL;
        return false;
    case YAML_SEQUENCE_START_EVENT:
        /* XXX: should we bail if there is a tag? */
        *type = RBH_VT_SEQUENCE;
        return true;
    case YAML_SCALAR_EVENT:
        break;
    default:
        errno = EINVAL;
        return false;
    }

    tag = yaml_scalar_tag(event);
    if (tag == NULL) {
        *type = RBH_VT_STRING;
        return true;
    }

    /* XXX: if this ever becomes a bottleneck, you can try unrolling the calls
     *      to strcmp() with large switch statements. Right now, this feels like
     *      premature optimization (the readability is just not as good).
     */
    if (strcmp(tag, UINT32_TAG) == 0) {
        *type = RBH_VT_UINT32;
        return true;
    }
    if (strcmp(tag, UINT64_TAG) == 0) {
        *type = RBH_VT_UINT64;
        return true;
    }
    if (strcmp(tag, INT32_TAG) == 0) {
        *type = RBH_VT_INT32;
        return true;
    }
    if (strcmp(tag, INT64_TAG) == 0) {
        *type = RBH_VT_INT64;
        return true;
    }
    if (strcmp(tag, YAML_BINARY_TAG) == 0) {
        *type = RBH_VT_BINARY;
        return true;
    }
    if (strcmp(tag, YAML_STR_TAG) == 0) {
        *type = RBH_VT_STRING;
        return true;
    }

    errno = EINVAL;
    return false;
}

/*---------------------------------- value -----------------------------------*/

static bool
emit_rbh_value(yaml_emitter_t *emitter, const struct rbh_value *value)
{
    switch (value->type) {
    case RBH_VT_BINARY:
        return yaml_emit_binary(emitter, value->binary.data,
                                value->binary.size);
    case RBH_VT_UINT32:
        return emit_uint32(emitter, value->uint32);
    case RBH_VT_UINT64:
        return emit_uint64(emitter, value->uint64);
    case RBH_VT_INT32:
        return emit_int32(emitter, value->int32);
    case RBH_VT_INT64:
        return emit_int64(emitter, value->int64);
    case RBH_VT_STRING:
        return YAML_EMIT_STRING(emitter, value->string);
    case RBH_VT_REGEX:
        return emit_regex(emitter, value->regex.string, value->regex.options);
    case RBH_VT_SEQUENCE:
        if (!yaml_emit_sequence_start(emitter, NULL))
            return false;

        for (size_t i = 0; i < value->sequence.count; i++) {
            if (!emit_rbh_value(emitter, &value->sequence.values[i]))
                return false;
        }

        return yaml_emit_sequence_end(emitter);
    case RBH_VT_MAP:
        return emit_rbh_value_map(emitter, &value->map);
    default:
        errno = EINVAL;
        return false;
    }
}

/* This function takes the ownership of `event' */
static bool
parse_rbh_value(yaml_parser_t *parser, yaml_event_t *event,
                struct rbh_value *value)
{
    bool success = false;

    if (!parse_value_type(event, &value->type)) {
        yaml_event_delete(event);
        return false;
    }

    switch (value->type) {
    case RBH_VT_BINARY:
        /* FIXME: this relies on libyaml's internals, it is a hack */
        value->binary.data = yaml_scalar_value(event);
        if (yaml_parse_binary(event, (char *)event->data.scalar.value,
                              &value->binary.size))
            goto save_event;
        break;
    case RBH_VT_UINT32:
        success = parse_uint32(event, &value->uint32);
        break;
    case RBH_VT_UINT64:
        success = parse_uint64(event, &value->uint64);
        break;
    case RBH_VT_INT32:
        success = parse_int32(event, &value->int32);
        break;
    case RBH_VT_INT64:
        success = parse_int64(event, &value->int64);
        break;
    case RBH_VT_STRING:
        if (yaml_parse_string(event, &value->string, NULL))
            goto save_event;
        break;
    case RBH_VT_REGEX:
        yaml_event_delete(event);
        return parse_regex_mapping(parser, &value->regex.string,
                                   &value->regex.options);
    case RBH_VT_SEQUENCE:
        yaml_event_delete(event);
        return parse_sequence(parser, value);
    case RBH_VT_MAP:
        yaml_event_delete(event);
        return parse_rbh_value_map(parser, &value->map);
    }

    yaml_event_delete(event);
    return success;

save_event:
    if (rbh_sstack_push(context.events, event, sizeof(*event)) == NULL) {
        int save_errno = errno;

        yaml_event_delete(event);
        errno = save_errno;
        return false;
    }

    return true;
}

/*----------------------------------------------------------------------------*
 |                                   xattrs                                   |
 *----------------------------------------------------------------------------*/

static bool
parse_xattrs(yaml_parser_t *parser, struct rbh_value_map *map)
{
    yaml_event_t event;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    if (event.type != YAML_MAPPING_START_EVENT) {
        yaml_event_delete(&event);
        errno = EINVAL;
        return false;
    }
    yaml_event_delete(&event);

    return parse_rbh_value_map(parser, map);
}

/*----------------------------------------------------------------------------*
 |                                   upsert                                   |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                               statx                                |
     *--------------------------------------------------------------------*/

static bool
emit_statx(yaml_emitter_t *emitter __attribute__((unused)),
           const struct statx *statxbuf __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

static bool
parse_statx(yaml_parser_t *parser __attribute__((unused)),
            struct statx *statxbuf __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

    /*--------------------------------------------------------------------*
     |                              symlink                               |
     *--------------------------------------------------------------------*/

static bool
parse_symlink(yaml_parser_t *parser __attribute__((unused)),
              const char **symlink __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

/*---------------------------------- upsert ----------------------------------*/

#define UPSERT_TAG "!upsert"

static bool
emit_upsert(yaml_emitter_t *emitter, const struct rbh_fsevent *upsert)
{
    const struct statx *statxbuf = upsert->upsert.statx;
    const char *symlink = upsert->upsert.symlink;

    return yaml_emit_mapping_start(emitter, UPSERT_TAG)
        && YAML_EMIT_STRING(emitter, "id")
        && yaml_emit_binary(emitter, upsert->id.data, upsert->id.size)
        && YAML_EMIT_STRING(emitter, "xattrs")
        && emit_rbh_value_map(emitter, &upsert->xattrs)
        && (statxbuf ? (YAML_EMIT_STRING(emitter, "statx")
                     && emit_statx(emitter, statxbuf))
                     : true)
        && (symlink ? YAML_EMIT_STRING(emitter, "symlink")
                   && YAML_EMIT_STRING(emitter, symlink) : true)
        && yaml_emit_mapping_end(emitter);
}

enum upsert_field {
    UF_UNKNOWN,
    UF_ID,
    UF_XATTRS,
    UF_STATX,
    UF_SYMLINK,
};

static enum upsert_field
str2upsert_field(const char *string)
{
    switch (*string++) {
    case 'i': /* id */
        if (strcmp(string, "d"))
            break;
        return UF_ID;
    case 's': /* statx, symlink */
        switch (*string++) {
        case 't': /* statx */
            if (strcmp(string, "atx"))
                break;
            return UF_STATX;
        case 'y': /* symlink */
            if (strcmp(string, "mlink"))
                break;
            return UF_SYMLINK;
        }
        break;
    case 'x': /* xattrs */
        if (strcmp(string, "attrs"))
            break;
        return UF_XATTRS;
    }

    errno = EINVAL;
    return UF_UNKNOWN;
}

static bool
parse_upsert(yaml_parser_t *parser, struct rbh_fsevent *upsert)
{
    static struct statx statxbuf;
    struct {
        bool id:1;
    } seen = {};

    while (true) {
        enum upsert_field field;
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

        field = str2upsert_field(key);
        save_errno = errno;
        yaml_event_delete(&event);

        switch (field) {
        case UF_UNKNOWN:
            errno = save_errno;
            return false;
        case UF_ID:
            success = parse_id(parser, &upsert->id);
            seen.id = true;
            break;
        case UF_XATTRS:
            success = parse_xattrs(parser, &upsert->xattrs);
            break;
        case UF_STATX:
            success = parse_statx(parser, &statxbuf);
            upsert->upsert.statx = &statxbuf;
            break;
        case UF_SYMLINK:
            success = parse_symlink(parser, &upsert->upsert.symlink);
            break;
        }

        if (!success)
            return false;
    }

    return seen.id;
}

/*----------------------------------------------------------------------------*
 |                                    link                                    |
 *----------------------------------------------------------------------------*/

#define LINK_TAG "!link"

static bool
emit_link(yaml_emitter_t *emitter __attribute__((unused)),
          const struct rbh_fsevent *link __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

static bool
parse_link(yaml_parser_t *parser __attribute__((unused)),
           struct rbh_fsevent *link __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

/*----------------------------------------------------------------------------*
 |                                   unlink                                   |
 *----------------------------------------------------------------------------*/

#define UNLINK_TAG "!unlink"

static bool
emit_unlink(yaml_emitter_t *emitter __attribute__((unused)),
            const struct rbh_fsevent *unlink __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

static bool
parse_unlink(yaml_parser_t *parser __attribute__((unused)),
             struct rbh_fsevent *unlink __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

/*----------------------------------------------------------------------------*
 |                                   delete                                   |
 *----------------------------------------------------------------------------*/

#define DELETE_TAG "!delete"

static bool
emit_delete(yaml_emitter_t *emitter __attribute__((unused)),
            const struct rbh_fsevent *delete __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

static bool
parse_delete(yaml_parser_t *parser __attribute__((unused)),
             struct rbh_fsevent *delete __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

/*----------------------------------------------------------------------------*
 |                                   xattr                                    |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                             namespace                              |
     *--------------------------------------------------------------------*/

#define NS_XATTR_TAG "!ns_xattr"

static bool
parse_ns_xattr(yaml_parser_t *parser __attribute__((unused)),
               struct rbh_fsevent *ns_xattr __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

    /*--------------------------------------------------------------------*
     |                               inode                                |
     *--------------------------------------------------------------------*/

#define INODE_XATTR_TAG "!inode_xattr"

static bool
parse_inode_xattr(yaml_parser_t *parser __attribute__((unused)),
                  struct rbh_fsevent *inode_xattr __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

/*---------------------------------- xattr -----------------------------------*/

static bool
emit_xattr(yaml_emitter_t *emitter __attribute__((unused)),
           const struct rbh_fsevent *xattr __attribute__((unused)))
{
    error(EXIT_FAILURE, ENOSYS, __func__);
    __builtin_unreachable();
}

/*----------------------------------------------------------------------------*
 |                                  fsevent                                   |
 *----------------------------------------------------------------------------*/

bool
emit_fsevent(yaml_emitter_t *emitter, const struct rbh_fsevent *fsevent)
{
    bool success;

    if (!yaml_emit_document_start(emitter))
        return false;

    switch ((int)fsevent->type) {
    case RBH_FET_UPSERT:
        success = emit_upsert(emitter, fsevent);
        break;
    case RBH_FET_LINK:
        success = emit_link(emitter, fsevent);
        break;
    case RBH_FET_UNLINK:
        success = emit_unlink(emitter, fsevent);
        break;
    case RBH_FET_DELETE:
        success = emit_delete(emitter, fsevent);
        break;
    case RBH_FET_XATTR:
        success = emit_xattr(emitter, fsevent);
        break;
    default:
        error(EXIT_FAILURE, EINVAL, __func__);
        __builtin_unreachable();
    }

    return success && yaml_emit_document_end(emitter);
}

enum fsevent_type {
    FT_UNKNOWN,
    FT_UPSERT,
    FT_DELETE,
    FT_LINK,
    FT_UNLINK,
    FT_NS_XATTR,
    FT_INODE_XATTR,
};

static enum fsevent_type
str2fsevent_type(const char *string)
{
    /* XXX: if this ever becomes a bottleneck, you can try unrolling the calls
     *      to strcmp() with large switch statements. Right now, this feels like
     *      premature optimization (the readability is just not as good).
     */
    if (strcmp(string, UPSERT_TAG) == 0)
        return FT_UPSERT;
    if (strcmp(string, DELETE_TAG) == 0)
        return FT_DELETE;
    if (strcmp(string, LINK_TAG) == 0)
        return FT_LINK;
    if (strcmp(string, UNLINK_TAG) == 0)
        return FT_UNLINK;
    if (strcmp(string, NS_XATTR_TAG) == 0)
        return FT_NS_XATTR;
    if (strcmp(string, INODE_XATTR_TAG) == 0)
        return FT_INODE_XATTR;

    errno = EINVAL;
    return FT_UNKNOWN;
}

bool
parse_fsevent(yaml_parser_t *parser, struct rbh_fsevent *fsevent)
{
    enum fsevent_type type;
    yaml_event_t event;
    const char *tag;
    int save_errno;

    context_reinit();

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
    case FT_UPSERT:
        fsevent->type = RBH_FET_UPSERT;
        return parse_upsert(parser, fsevent);
    case FT_DELETE:
        fsevent->type = RBH_FET_DELETE;
        return parse_delete(parser, fsevent);
    case FT_LINK:
        fsevent->type = RBH_FET_LINK;
        return parse_link(parser, fsevent);
    case FT_UNLINK:
        fsevent->type = RBH_FET_UNLINK;
        return parse_unlink(parser, fsevent);
    case FT_NS_XATTR:
        fsevent->type = RBH_FET_XATTR;
        return parse_ns_xattr(parser, fsevent);
    case FT_INODE_XATTR:
        fsevent->type = RBH_FET_XATTR;
        return parse_inode_xattr(parser, fsevent);
    default:
        assert(false);
        __builtin_unreachable();
    }
}
