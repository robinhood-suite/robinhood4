/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <errno.h>
#include <error.h>
#include <limits.h>
#include <stdlib.h>

#include <sys/stat.h>
# include <robinhood/statx.h>
#ifndef HAVE_STATX
# include <robinhood/statx-compat.h>
#endif

#include <miniyaml.h>
#include <robinhood.h>

#include "serialization.h"

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

static bool
parse_next_uint64(yaml_parser_t *parser, uint64_t *u)
{
    yaml_event_t event;
    int save_errno;
    bool success;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    if (event.type != YAML_SCALAR_EVENT) {
        yaml_event_delete(&event);
        errno = EINVAL;
        return false;
    }

    success = parse_uint64(&event, u);
    save_errno = errno;
    yaml_event_delete(&event);
    errno = save_errno;
    return success;
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

static bool
parse_next_uint32(yaml_parser_t *parser, uint32_t *u)
{
    yaml_event_t event;
    int save_errno;
    bool success;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    if (event.type != YAML_SCALAR_EVENT) {
        yaml_event_delete(&event);
        errno = EINVAL;
        return false;
    }

    success = parse_uint32(&event, u);
    save_errno = errno;
    yaml_event_delete(&event);
    errno = save_errno;
    return success;
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
    yaml_event_t map_event;
    size_t count = 1; /* TODO: fine tune this */
    bool end = false;
    size_t i = 0;

    if (!yaml_parser_parse(parser, &map_event))
        parser_error(parser);

    if (map_event.type == YAML_MAPPING_END_EVENT) {
        /* This is the case when the xattrs map is empty, and the YAML string is
         * '"xattrs": {}'.
         */
        map->pairs = NULL;
        map->count = 0;
        yaml_event_delete(&map_event);
        return true;
    } else if (map_event.type != YAML_MAPPING_START_EVENT) {
        yaml_event_delete(&map_event);
        errno = EINVAL;
        return false;
    }

    yaml_event_delete(&map_event);

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

enum regex_field {
    RF_UNKNOWN  = 0x0,
    RF_REGEX    = 0x1,
    RF_OPTIONS  = 0x2,
};

static enum regex_field
regex_field_tokenize(const char *key)
{
    switch (*key++) {
    case 'r': /* regex */
        if (strcmp(key, "egex"))
            break;
        return RF_REGEX;
    case 'o': /* options */
        if (strcmp(key, "ptions"))
            break;
        return RF_OPTIONS;
    }

    errno = EINVAL;
    return RF_UNKNOWN;
}

static void
parse_regex_field(yaml_parser_t *parser, const char *key, const char **regex,
                  unsigned int *options, unsigned int *seen)
{
    yaml_event_t event;
    uintmax_t umax;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    switch (regex_field_tokenize(key)) {
    case RF_UNKNOWN:
        break;
    case RF_REGEX:
        if (yaml_parse_string(&event, regex, NULL)) {
            /* Instead of making a copy of `regex', we are just going to keep
             * a copy of the event and free it later.
             */
            if (rbh_sstack_push(context.events, &event, sizeof(event)) == NULL)
                error(EXIT_FAILURE, errno, "rbh_sstack_push");
            *seen |= RF_REGEX;
            return;
        }
        break;
    case RF_OPTIONS:
        if (yaml_parse_unsigned_integer(&event, &umax)) {
            if (umax > UINT_MAX) {
                errno = ERANGE;
                break;
            }

            *options = umax;
            *seen |= RF_OPTIONS;
            yaml_event_delete(&event);
            return;
        }
        break;
    }

    error(0, errno, "%s, l.%zu:%zu", key, event.start_mark.line,
          event.start_mark.column);

    if (!yaml_parser_skip(parser, event.type))
        parser_error(parser);
    yaml_event_delete(&event);
}

static bool
parse_regex_mapping(yaml_parser_t *parser, const char **regex,
                    unsigned int *options)
{
    unsigned int seen = 0;
    bool end = false;

    *options = 0;

    do {
        yaml_event_t event;
        const char *key;

        if (!yaml_parser_parse(parser, &event))
            parser_error(parser);

        switch (event.type) {
        case YAML_MAPPING_END_EVENT:
            end = true;
            break;
        case YAML_SCALAR_EVENT:
            if (yaml_parse_string(&event, &key, NULL)) {
                parse_regex_field(parser, key, regex, options, &seen);
                break;
            }

            /* Otherwise, skip it */
            __attribute__((fallthrough));
        default:
            /* Ignore this key/value */
            if (!yaml_parser_skip(parser, event.type))
                parser_error(parser);
            if (!yaml_parser_skip_next(parser))
                parser_error(parser);
        }

        yaml_event_delete(&event);
    } while (!end);

    /* "options" is optional */
    return seen & RF_REGEX;
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
    if (strcmp(tag, YAML_BOOL_TAG) == 0) {
        *type = RBH_VT_BOOLEAN;
        return true;
    }
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
    if (value == NULL)
        return yaml_emit_null(emitter);

    switch (value->type) {
    case RBH_VT_BOOLEAN:
        return yaml_emit_boolean(emitter, value->boolean);
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
    case RBH_VT_BOOLEAN:
        success = yaml_parse_boolean(event, &value->boolean);
        break;
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

        /*------------------------------------------------------------*
         |                          filetype                          |
         *------------------------------------------------------------*/

static bool
emit_filetype(yaml_emitter_t *emitter, uint16_t filetype)
{
    switch (filetype) {
    case S_IFSOCK:
        return YAML_EMIT_STRING(emitter, "socket");
    case S_IFLNK:
        return YAML_EMIT_STRING(emitter, "link");
    case S_IFREG:
        return YAML_EMIT_STRING(emitter, "file");
    case S_IFBLK:
        return YAML_EMIT_STRING(emitter, "blockdev");
    case S_IFDIR:
        return YAML_EMIT_STRING(emitter, "directory");
    case S_IFCHR:
        return YAML_EMIT_STRING(emitter, "chardev");
    case S_IFIFO:
        return YAML_EMIT_STRING(emitter, "fifo");
    default:
        errno = EINVAL;
        return false;
    }
}

static int
str2filetype(const char *string)
{
    switch (*string++) {
    case 'b': /* blockdev */
        if (strcmp(string, "lockdev"))
            break;
        return S_IFBLK;
    case 'c': /* chardev */
        if (strcmp(string, "hardev"))
            break;
        return S_IFCHR;
    case 'd': /* directory */
        if (strcmp(string, "irectory"))
            break;
        return S_IFDIR;
    case 'f': /* fifo, file */
        if (*string++ != 'i')
            break;
        switch (*string++) {
        case 'f': /* fifo */
            if (strcmp(string, "o"))
                break;
            return S_IFIFO;
        case 'l': /* file */
            if (strcmp(string, "e"))
                break;
            return S_IFREG;
        }
        break;
    case 'l': /* link */
        if (strcmp(string, "ink"))
            break;
        return S_IFLNK;
    case 's': /* socket */
        if (strcmp(string, "ocket"))
            break;
        return S_IFSOCK;
    }

    errno = EINVAL;
    return 0;
}

static bool
parse_filetype(yaml_parser_t *parser, uint16_t *filetype)
{
    yaml_event_t event;
    const char *name;
    int save_errno;
    uint16_t type;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    if (!yaml_parse_string(&event, &name, NULL)) {
        save_errno = errno;
        yaml_event_delete(&event);
        errno = save_errno;
        return false;
    }

    type = str2filetype(name);
    save_errno = errno;
    yaml_event_delete(&event);
    if (type == 0) {
        errno = save_errno;
        return false;
    }

    *filetype = type;
    return true;
}

        /*------------------------------------------------------------*
         |                        permissions                         |
         *------------------------------------------------------------*/

static bool
emit_octal_unsigned_integer(yaml_emitter_t *emitter, uintmax_t u)
{
    char buffer[4 * sizeof(u)];
    int n;

    n = snprintf(buffer, sizeof(buffer), "0%" PRIoMAX, u);
    return yaml_emit_scalar(emitter, NULL, buffer, n, YAML_PLAIN_SCALAR_STYLE);
}

static bool
parse_permissions(yaml_parser_t *parser, uint16_t *permissions)
{
    yaml_event_t event;
    int save_errno;
    bool success;
    uintmax_t u;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    if (event.type != YAML_SCALAR_EVENT) {
        yaml_event_delete(&event);
        errno = EINVAL;
        return false;
    }

    success = yaml_parse_unsigned_integer(&event, &u);
    save_errno = errno;
    yaml_event_delete(&event);

    if (!success) {
        errno = save_errno;
        return false;
    }

    if (u > UINT16_MAX) {
        errno = ERANGE;
        return false;
    }

    *permissions = u;
    return true;
}

        /*------------------------------------------------------------*
         |                         attributes                         |
         *------------------------------------------------------------*/

static bool
emit_statx_attributes(yaml_emitter_t *emitter, uint64_t mask,
                      uint64_t attributes)
{
    if (!yaml_emit_mapping_start(emitter, NULL))
        return false;

    if (mask & RBH_STATX_ATTR_COMPRESSED) {
        if (!YAML_EMIT_STRING(emitter, "compressed")
         || !yaml_emit_boolean(emitter, attributes & RBH_STATX_ATTR_COMPRESSED))
            return false;
    }

    if (mask & RBH_STATX_ATTR_IMMUTABLE) {
        if (!YAML_EMIT_STRING(emitter, "immutable")
         || !yaml_emit_boolean(emitter, attributes & RBH_STATX_ATTR_IMMUTABLE))
            return false;
    }

    if (mask & RBH_STATX_ATTR_APPEND) {
        if (!YAML_EMIT_STRING(emitter, "append")
         || !yaml_emit_boolean(emitter, attributes & RBH_STATX_ATTR_APPEND))
            return false;
    }

    if (mask & RBH_STATX_ATTR_NODUMP) {
        if (!YAML_EMIT_STRING(emitter, "nodump")
         || !yaml_emit_boolean(emitter, attributes & RBH_STATX_ATTR_NODUMP))
            return false;
    }

    if (mask & RBH_STATX_ATTR_ENCRYPTED) {
        if (!YAML_EMIT_STRING(emitter, "encrypted")
         || !yaml_emit_boolean(emitter, attributes & RBH_STATX_ATTR_ENCRYPTED))
            return false;
    }

    if (mask & RBH_STATX_ATTR_AUTOMOUNT) {
        if (!YAML_EMIT_STRING(emitter, "automount")
         || !yaml_emit_boolean(emitter, attributes & RBH_STATX_ATTR_AUTOMOUNT))
            return false;
    }

    if (mask & RBH_STATX_ATTR_MOUNT_ROOT) {
        if (!YAML_EMIT_STRING(emitter, "mount-root")
         || !yaml_emit_boolean(emitter, attributes & RBH_STATX_ATTR_MOUNT_ROOT))
            return false;
    }

    if (mask & RBH_STATX_ATTR_VERITY) {
        if (!YAML_EMIT_STRING(emitter, "verity")
         || !yaml_emit_boolean(emitter, attributes & RBH_STATX_ATTR_VERITY))
            return false;
    }

    if (mask & RBH_STATX_ATTR_DAX) {
        if (!YAML_EMIT_STRING(emitter, "dax")
         || !yaml_emit_boolean(emitter, attributes & RBH_STATX_ATTR_DAX))
            return false;
    }

    return yaml_emit_mapping_end(emitter);
}

static uint64_t
str2statx_attribute(const char *string)
{
    switch (*string++) {
    case 'a': /* append, automount */
        switch (*string++) {
        case 'p': /* append */
            if (strcmp(string, "pend"))
                break;
            return RBH_STATX_ATTR_APPEND;
        case 'u': /* automount */
            if (strcmp(string, "tomount"))
                break;
            return RBH_STATX_ATTR_AUTOMOUNT;
        }
        break;
    case 'c': /* compressed */
        if (strcmp(string, "ompressed"))
            break;
        return RBH_STATX_ATTR_COMPRESSED;
    case 'd': /* dax */
        if (strcmp(string, "ax"))
            break;
        return RBH_STATX_ATTR_DAX;
    case 'e': /* encrypted */
        if (strcmp(string, "ncrypted"))
            break;
        return RBH_STATX_ATTR_ENCRYPTED;
    case 'i': /* immutable */
        if (strcmp(string, "mmutable"))
            break;
        return RBH_STATX_ATTR_IMMUTABLE;
    case 'm': /* mount-root */
        if (strcmp(string, "ount-root"))
            break;
        return RBH_STATX_ATTR_MOUNT_ROOT;
    case 'n': /* nodump */
        if (strcmp(string, "odump"))
            break;
        return RBH_STATX_ATTR_NODUMP;
    case 'v': /* verity */
        if (strcmp(string, "erity"))
            break;
        return RBH_STATX_ATTR_VERITY;
    }

    errno = EINVAL;
    return 0;
}

static bool
_parse_statx_attributes(yaml_parser_t *parser, uint64_t *mask,
                        uint64_t *attributes)
{
    *mask = 0;

    while (true) {
        yaml_event_t event;
        const char *name;
        int save_errno;
        uint64_t attr;
        bool success;
        bool is_set;

        if (!yaml_parser_parse(parser, &event))
            parser_error(parser);

        if (event.type == YAML_MAPPING_END_EVENT) {
            yaml_event_delete(&event);
            return true;
        }

        if (!yaml_parse_string(&event, &name, NULL)) {
            save_errno = errno;
            yaml_event_delete(&event);
            errno = save_errno;
            return false;
        }

        attr = str2statx_attribute(name);
        save_errno = errno;
        yaml_event_delete(&event);
        if (attr == 0) {
            errno = save_errno;
            return false;
        }

        *mask |= attr;

        if (!yaml_parser_parse(parser, &event))
            parser_error(parser);

        success = yaml_parse_boolean(&event, &is_set);
        save_errno = errno;
        yaml_event_delete(&event);
        if (!success) {
            errno = save_errno;
            return false;
        }

        if (is_set)
            *attributes |= attr;
        else
            *attributes &= ~attr;
    }
}

static bool
parse_statx_attributes(yaml_parser_t *parser, uint64_t *mask,
                       uint64_t *attributes)
{
    yaml_event_type_t type;
    yaml_event_t event;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    type = event.type;
    yaml_event_delete(&event);

    if (type != YAML_MAPPING_START_EVENT) {
        errno = EINVAL;
        return false;
    }

    return _parse_statx_attributes(parser, mask, attributes);
}

        /*------------------------------------------------------------*
         |                           nlink                            |
         *------------------------------------------------------------*/

static bool
parse_nlink(yaml_parser_t *parser, uint32_t *nlink)
{
    return parse_next_uint32(parser, nlink);
}

        /*------------------------------------------------------------*
         |                            uid                             |
         *------------------------------------------------------------*/

static bool
parse_uid(yaml_parser_t *parser, uint32_t *uid)
{
    return parse_next_uint32(parser, uid);
}

        /*------------------------------------------------------------*
         |                            gid                             |
         *------------------------------------------------------------*/

static bool
parse_gid(yaml_parser_t *parser, uint32_t *gid)
{
    return parse_next_uint32(parser, gid);
}

        /*------------------------------------------------------------*
         |                            ino                             |
         *------------------------------------------------------------*/

static bool
parse_ino(yaml_parser_t *parser, uint64_t *ino)
{
    return parse_next_uint64(parser, ino);
}

        /*------------------------------------------------------------*
         |                            size                            |
         *------------------------------------------------------------*/

static bool
parse_size(yaml_parser_t *parser, uint64_t *size)
{
    return parse_next_uint64(parser, size);
}

        /*------------------------------------------------------------*
         |                           blocks                           |
         *------------------------------------------------------------*/

static bool
parse_blocks(yaml_parser_t *parser, uint64_t *blocks)
{
    return parse_next_uint64(parser, blocks);
}

        /*------------------------------------------------------------*
         |                          blksize                           |
         *------------------------------------------------------------*/

static bool
parse_blksize(yaml_parser_t *parser, uint32_t *blksize)
{
    return parse_next_uint32(parser, blksize);
}

        /*------------------------------------------------------------*
         |                         timestamp                          |
         *------------------------------------------------------------*/

static bool
emit_statx_timestamp(yaml_emitter_t *emitter, bool sec, bool nsec,
                     const struct rbh_statx_timestamp *timestamp)
{
    return yaml_emit_mapping_start(emitter, NULL)
        && (sec ? YAML_EMIT_STRING(emitter, "sec")
               && yaml_emit_integer(emitter, timestamp->tv_sec) : true)
        && (nsec ? YAML_EMIT_STRING(emitter, "nsec")
                && yaml_emit_unsigned_integer(emitter, timestamp->tv_nsec)
                 : true)
        && yaml_emit_mapping_end(emitter);
}

enum statx_timestamp_field {
    STF_UNKNOWN,
    STF_SEC,
    STF_NSEC,
};

static enum statx_timestamp_field
str2statx_timestamp_field(const char *string)
{
    switch (*string++) {
    case 'n': /* nsec */
        if (strcmp(string, "sec"))
            break;
        return STF_NSEC;
    case 's': /* sec */
        if (strcmp(string, "ec"))
            break;
        return STF_SEC;
    }

    errno = EINVAL;
    return STF_UNKNOWN;
}

static bool
_parse_statx_timestamp(yaml_parser_t *parser, uint32_t *mask, uint32_t sec,
                       uint32_t nsec, struct rbh_statx_timestamp *timestamp)
{
    while (true) {
        enum statx_timestamp_field field;
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

        field = str2statx_timestamp_field(key);
        save_errno = errno;
        yaml_event_delete(&event);
        switch (field) {
        case STF_UNKNOWN:
            errno = save_errno;
            return false;
        case STF_SEC:
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            static_assert(sizeof(timestamp->tv_sec) == sizeof(int64_t), "");
            success = parse_int64(&event, (int64_t *)&timestamp->tv_sec);
            save_errno = errno;
            yaml_event_delete(&event);
            if (!success) {
                errno = save_errno;
                return false;
            }
            *mask |= sec;
            break;
        case STF_NSEC:
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            success = parse_uint32(&event, &timestamp->tv_nsec);
            save_errno = errno;
            yaml_event_delete(&event);
            if (!success) {
                errno = save_errno;
                return false;
            }
            *mask |= nsec;
            break;
        }
    }

    return true;
}

static bool
parse_statx_timestamp(yaml_parser_t *parser, uint32_t *mask, uint32_t sec,
                      uint32_t nsec, struct rbh_statx_timestamp *timestamp)
{
    yaml_event_type_t type;
    yaml_event_t event;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    type = event.type;
    yaml_event_delete(&event);

    if (type != YAML_MAPPING_START_EVENT) {
        errno = EINVAL;
        return false;
    }

    return _parse_statx_timestamp(parser, mask, sec, nsec, timestamp);
}

        /*------------------------------------------------------------*
         |                       device numbers                       |
         *------------------------------------------------------------*/

static bool
emit_device_number(yaml_emitter_t *emitter, bool major_, uint32_t major,
                   bool minor_, uint32_t minor)
{
    return yaml_emit_mapping_start(emitter, NULL)
        && (major_ ? YAML_EMIT_STRING(emitter, "major")
                  && yaml_emit_unsigned_integer(emitter, major) : true)
        && (minor_ ? YAML_EMIT_STRING(emitter, "minor")
                  && yaml_emit_unsigned_integer(emitter, minor) : true)
        && yaml_emit_mapping_end(emitter);
}

enum device_number_field {
    DNF_UNKNOWN,
    DNF_MAJOR,
    DNF_MINOR,
};

static enum device_number_field
str2device_numbers(const char *string)
{
    switch (*string++) {
    case 'm': /* major, minor */
        switch (*string++) {
        case 'a': /* major */
            if (strcmp(string, "jor"))
                break;
            return DNF_MAJOR;
        case 'i': /* minor */
            if (strcmp(string, "nor"))
                break;
            return DNF_MINOR;
        }
        break;
    }

    errno = EINVAL;
    return DNF_UNKNOWN;
}

static bool
_parse_device_numbers(yaml_parser_t *parser, uint32_t *mask, uint32_t major_,
                      uint32_t *major, uint32_t minor_, uint32_t *minor)
{
    while (true) {
        enum device_number_field field;
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

        field = str2device_numbers(key);
        save_errno = errno;
        yaml_event_delete(&event);
        switch (field) {
        case DNF_UNKNOWN:
            errno = save_errno;
            return false;
        case DNF_MAJOR:
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            success = parse_uint32(&event, major);
            save_errno = errno;
            yaml_event_delete(&event);
            if (!success) {
                errno = save_errno;
                return false;
            }
            *mask |= major_;
            break;
        case DNF_MINOR:
            if (!yaml_parser_parse(parser, &event))
                parser_error(parser);

            success = parse_uint32(&event, minor);
            save_errno = errno;
            yaml_event_delete(&event);
            if (!success) {
                errno = save_errno;
                return false;
            }
            *mask |= minor_;
            break;
        }
    }

    return true;
}

static bool
parse_device_numbers(yaml_parser_t *parser, uint32_t *mask, uint32_t major_,
                      uint32_t *major, uint32_t minor_, uint32_t *minor)
{
    yaml_event_type_t type;
    yaml_event_t event;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    type = event.type;
    yaml_event_delete(&event);

    if (type != YAML_MAPPING_START_EVENT) {
        errno = EINVAL;
        return false;
    }

    return _parse_device_numbers(parser, mask, major_, major, minor_, minor);
}

    /*------------------------------ statx -------------------------------*/

static bool
emit_statx(yaml_emitter_t *emitter, const struct rbh_statx *statxbuf)
{
    uint64_t mask = statxbuf->stx_mask;

    if (!yaml_emit_mapping_start(emitter, NULL))
        return false;

    if (mask & RBH_STATX_TYPE) {
        if (!YAML_EMIT_STRING(emitter, "type")
         || !emit_filetype(emitter, statxbuf->stx_mode & S_IFMT))
            return false;
    }

    if (mask & RBH_STATX_MODE) {
        if (!YAML_EMIT_STRING(emitter, "mode")
         || !emit_octal_unsigned_integer(emitter, statxbuf->stx_mode & ~S_IFMT))
            return false;
    }

    if (mask & RBH_STATX_NLINK) {
        if (!YAML_EMIT_STRING(emitter, "nlink")
         || !yaml_emit_unsigned_integer(emitter, statxbuf->stx_nlink))
            return false;
    }

    if (mask & RBH_STATX_UID) {
        if (!YAML_EMIT_STRING(emitter, "uid")
         || !yaml_emit_unsigned_integer(emitter, statxbuf->stx_uid))
            return false;
    }

    if (mask & RBH_STATX_GID) {
        if (!YAML_EMIT_STRING(emitter, "gid")
         || !yaml_emit_unsigned_integer(emitter, statxbuf->stx_gid))
            return false;
    }

    if (mask & RBH_STATX_ATIME) {
        if (!YAML_EMIT_STRING(emitter, "atime")
         || !emit_statx_timestamp(emitter, mask & RBH_STATX_ATIME_SEC,
                                  mask & RBH_STATX_ATIME_NSEC,
                                  &statxbuf->stx_atime))
            return false;
    }

    if (mask & RBH_STATX_MTIME) {
        if (!YAML_EMIT_STRING(emitter, "mtime")
         || !emit_statx_timestamp(emitter, mask & RBH_STATX_MTIME_SEC,
                                  mask & RBH_STATX_MTIME_NSEC,
                                  &statxbuf->stx_mtime))
            return false;
    }

    if (mask & RBH_STATX_CTIME) {
        if (!YAML_EMIT_STRING(emitter, "ctime")
         || !emit_statx_timestamp(emitter, mask & RBH_STATX_CTIME_SEC,
                                  mask & RBH_STATX_CTIME_NSEC,
                                  &statxbuf->stx_ctime))
            return false;
    }

    if (mask & RBH_STATX_INO) {
        if (!YAML_EMIT_STRING(emitter, "ino")
         || !yaml_emit_unsigned_integer(emitter, statxbuf->stx_ino))
            return false;
    }

    if (mask & RBH_STATX_SIZE) {
        if (!YAML_EMIT_STRING(emitter, "size")
         || !yaml_emit_unsigned_integer(emitter, statxbuf->stx_size))
            return false;
    }

    if (mask & RBH_STATX_BLOCKS) {
        if (!YAML_EMIT_STRING(emitter, "blocks")
         || !yaml_emit_unsigned_integer(emitter, statxbuf->stx_blocks))
            return false;
    }

    if (mask & RBH_STATX_BTIME) {
        if (!YAML_EMIT_STRING(emitter, "btime")
         || !emit_statx_timestamp(emitter, mask & RBH_STATX_BTIME_SEC,
                                  mask & RBH_STATX_BTIME_NSEC,
                                  &statxbuf->stx_btime))
            return false;
    }

    if (mask & RBH_STATX_BLKSIZE) {
        if (!YAML_EMIT_STRING(emitter, "blksize")
         || !yaml_emit_unsigned_integer(emitter, statxbuf->stx_blksize))
            return false;
    }

    if (mask & RBH_STATX_ATTRIBUTES) {
        if (!YAML_EMIT_STRING(emitter, "attributes")
         || !emit_statx_attributes(emitter, statxbuf->stx_attributes_mask,
                                   statxbuf->stx_attributes))
            return false;
    }

    if (mask & RBH_STATX_RDEV) {
        if (!YAML_EMIT_STRING(emitter, "rdev")
         || !emit_device_number(emitter, mask & RBH_STATX_RDEV_MAJOR,
                                statxbuf->stx_rdev_major,
                                mask & RBH_STATX_RDEV_MINOR,
                                statxbuf->stx_rdev_minor))
            return false;
    }

    if (mask & RBH_STATX_DEV) {
        if (!YAML_EMIT_STRING(emitter, "dev")
         || !emit_device_number(emitter, mask & RBH_STATX_DEV_MAJOR,
                                statxbuf->stx_dev_major,
                                mask & RBH_STATX_DEV_MINOR,
                                statxbuf->stx_dev_minor))
            return false;
    }

    return yaml_emit_mapping_end(emitter);
}

enum statx_field {
    SF_UNKNOWN,
    SF_TYPE,
    SF_MODE,
    SF_NLINK,
    SF_UID,
    SF_GID,
    SF_ATIME,
    SF_MTIME,
    SF_CTIME,
    SF_INO,
    SF_SIZE,
    SF_BLOCKS,
    SF_BTIME,
    SF_BLKSIZE,
    SF_ATTRIBUTES,
    SF_RDEV,
    SF_DEV,
};

static enum statx_field
str2statx_field(const char *string)
{
    switch (*string++) {
    case 'a': /* atime, attributes */
        if (*string++ != 't')
            break;
        switch (*string++) {
        case 'i': /* atime */
            if (strcmp(string, "me"))
                break;
            return SF_ATIME;
        case 't': /* attributes */
            if (strcmp(string, "ributes"))
                break;
            return SF_ATTRIBUTES;
        }
        break;
    case 'b': /* blksize, blocks, btime */
        switch (*string++) {
        case 'l': /* blksize, blocks */
            switch (*string++) {
            case 'k': /* blksize */
                if (strcmp(string, "size"))
                    break;
                return SF_BLKSIZE;
            case 'o': /* blocks */
                if (strcmp(string, "cks"))
                    break;
                return SF_BLOCKS;
            }
            break;
        case 't': /* btime */
            if (strcmp(string, "ime"))
                break;
            return SF_BTIME;
        }
        break;
    case 'c': /* ctime */
        if (strcmp(string, "time"))
            break;
        return SF_CTIME;
    case 'd': /* dev */
        if (strcmp(string, "ev"))
            break;
        return SF_DEV;
    case 'g': /* gid */
        if (strcmp(string, "id"))
            break;
        return SF_GID;
    case 'i': /* ino */
        if (strcmp(string, "no"))
            break;
        return SF_INO;
    case 'm': /* mode, mtime */
        switch (*string++) {
        case 'o': /* mode */
            if (strcmp(string, "de"))
                break;
            return SF_MODE;
        case 't': /* mtime */
            if (strcmp(string, "ime"))
                break;
            return SF_MTIME;
        }
        break;
    case 'n': /* nlink */
        if (strcmp(string, "link"))
            break;
        return SF_NLINK;
    case 'r': /* rdev */
        if (strcmp(string, "dev"))
            break;
        return SF_RDEV;
    case 's': /* size */
        if (strcmp(string, "ize"))
            break;
        return SF_SIZE;
    case 't': /* type */
        if (strcmp(string, "ype"))
            break;
        return SF_TYPE;
    case 'u': /* uid */
        if (strcmp(string, "id"))
            break;
        return SF_UID;
    }

    errno = EINVAL;
    return SF_UNKNOWN;
}

static bool
parse_statx_mapping(yaml_parser_t *parser, struct rbh_statx *statxbuf)
{
    statxbuf->stx_mode = 0;

    while (true) {
        enum statx_field field;
        yaml_event_t event;
        const char *key;
        int save_errno;
        bool success;
        uint16_t tmp;

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

        field = str2statx_field(key);
        save_errno = errno;
        yaml_event_delete(&event);

        switch (field) {
        case SF_UNKNOWN:
            errno = save_errno;
            return false;
        case SF_TYPE:
            statxbuf->stx_mask |= RBH_STATX_TYPE;
            success = parse_filetype(parser, &tmp);
            if (success)
                statxbuf->stx_mode |= tmp;
            break;
        case SF_MODE:
            statxbuf->stx_mask |= RBH_STATX_MODE;
            success = parse_permissions(parser, &tmp);
            if (success)
                statxbuf->stx_mode |= tmp;
            break;
        case SF_NLINK:
            statxbuf->stx_mask |= RBH_STATX_NLINK;
            success = parse_nlink(parser, &statxbuf->stx_nlink);
            break;
        case SF_UID:
            statxbuf->stx_mask |= RBH_STATX_UID;
            success = parse_uid(parser, &statxbuf->stx_uid);
            break;
        case SF_GID:
            statxbuf->stx_mask |= RBH_STATX_GID;
            success = parse_gid(parser, &statxbuf->stx_gid);
            break;
        case SF_ATIME:
            success = parse_statx_timestamp(parser, &statxbuf->stx_mask,
                                            RBH_STATX_ATIME_SEC,
                                            RBH_STATX_ATIME_NSEC,
                                            &statxbuf->stx_atime);
            break;
        case SF_MTIME:
            success = parse_statx_timestamp(parser, &statxbuf->stx_mask,
                                            RBH_STATX_MTIME_SEC,
                                            RBH_STATX_MTIME_NSEC,
                                            &statxbuf->stx_mtime);
            break;
        case SF_CTIME:
            success = parse_statx_timestamp(parser, &statxbuf->stx_mask,
                                            RBH_STATX_CTIME_SEC,
                                            RBH_STATX_CTIME_NSEC,
                                            &statxbuf->stx_ctime);
            break;
        case SF_INO:
            statxbuf->stx_mask |= RBH_STATX_INO;
            static_assert(sizeof(statxbuf->stx_size) == sizeof(uint64_t), "");
            success = parse_ino(parser, (uint64_t *)&statxbuf->stx_ino);
            break;
        case SF_SIZE:
            statxbuf->stx_mask |= RBH_STATX_SIZE;
            static_assert(sizeof(statxbuf->stx_size) == sizeof(uint64_t), "");
            success = parse_size(parser, (uint64_t *)&statxbuf->stx_size);
            break;
        case SF_BLOCKS:
            statxbuf->stx_mask |= RBH_STATX_BLOCKS;
            static_assert(sizeof(statxbuf->stx_blocks) == sizeof(uint64_t), "");
            success = parse_blocks(parser, (uint64_t *)&statxbuf->stx_blocks);
            break;
        case SF_BTIME:
            success = parse_statx_timestamp(parser, &statxbuf->stx_mask,
                                            RBH_STATX_BTIME_SEC,
                                            RBH_STATX_BTIME_NSEC,
                                            &statxbuf->stx_btime);
            break;
        case SF_BLKSIZE:
            statxbuf->stx_mask |= RBH_STATX_BLKSIZE;
            success = parse_blksize(parser, &statxbuf->stx_blksize);
            break;
        case SF_ATTRIBUTES:
            /* XXX: gcc does not allow a static assert to immediately follow
             *      a label... Beats me.
             */
            ;
            static_assert(
                    sizeof(statxbuf->stx_attributes_mask) == sizeof(uint64_t),
                    ""
                    );
            static_assert(sizeof(statxbuf->stx_attributes) == sizeof(uint64_t),
                          "");
            statxbuf->stx_mask |= RBH_STATX_ATTRIBUTES;
            success = parse_statx_attributes(
                    parser, (uint64_t *)&statxbuf->stx_attributes_mask,
                    (uint64_t *)&statxbuf->stx_attributes
                    );
            break;
        case SF_RDEV:
            success = parse_device_numbers(parser, &statxbuf->stx_mask,
                                           RBH_STATX_RDEV_MAJOR,
                                           &statxbuf->stx_rdev_major,
                                           RBH_STATX_RDEV_MINOR,
                                           &statxbuf->stx_rdev_minor);
            break;
        case SF_DEV:
            success = parse_device_numbers(parser, &statxbuf->stx_mask,
                                           RBH_STATX_DEV_MAJOR,
                                           &statxbuf->stx_dev_major,
                                           RBH_STATX_DEV_MINOR,
                                           &statxbuf->stx_dev_minor);
            break;
        }

        if (!success)
            return false;
    }

    return true;
}

static bool
parse_statx(yaml_parser_t *parser, struct rbh_statx *statxbuf)
{
    yaml_event_type_t type;
    yaml_event_t event;
    bool success;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    type = event.type;
    yaml_event_delete(&event);

    switch (type) {
    case YAML_MAPPING_START_EVENT:
        success = parse_statx_mapping(parser, statxbuf);
        break;
    default:
        errno = EINVAL;
        return false;
    }

    return success;
}

    /*--------------------------------------------------------------------*
     |                              symlink                               |
     *--------------------------------------------------------------------*/

static bool
parse_symlink(yaml_parser_t *parser, const char **symlink)
{
    yaml_event_t event;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    if (event.type != YAML_SCALAR_EVENT) {
        yaml_event_delete(&event);
        errno = EINVAL;
        return false;
    }

    if (!yaml_parse_string(&event, symlink, NULL)) {
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

/*---------------------------------- upsert ----------------------------------*/

#define UPSERT_TAG "!upsert"

static bool
emit_upsert(yaml_emitter_t *emitter, const struct rbh_fsevent *upsert)
{
    const struct rbh_statx *statxbuf = upsert->upsert.statx;
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
    static struct rbh_statx statxbuf;
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

    /*--------------------------------------------------------------------*
     |                                name                                |
     *--------------------------------------------------------------------*/

static bool
parse_name(yaml_parser_t *parser, const char **name)
{
    yaml_event_t event;

    if (!yaml_parser_parse(parser, &event))
        parser_error(parser);

    if (event.type != YAML_SCALAR_EVENT) {
        yaml_event_delete(&event);
        errno = EINVAL;
        return false;
    }

    if (!yaml_parse_string(&event, name, NULL)) {
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

/*----------------------------------- link -----------------------------------*/

#define LINK_TAG "!link"

static bool
emit_link(yaml_emitter_t *emitter, const struct rbh_fsevent *link)
{
    const struct rbh_id *parent = link->link.parent_id;
    const char *name = link->link.name;

    return yaml_emit_mapping_start(emitter, LINK_TAG)
        && YAML_EMIT_STRING(emitter, "id")
        && yaml_emit_binary(emitter, link->id.data, link->id.size)
        && YAML_EMIT_STRING(emitter, "xattrs")
        && emit_rbh_value_map(emitter, &link->xattrs)
        && YAML_EMIT_STRING(emitter, "parent")
        && yaml_emit_binary(emitter, parent->data, parent->size)
        && YAML_EMIT_STRING(emitter, "name")
        && YAML_EMIT_STRING(emitter, name)
        && yaml_emit_mapping_end(emitter);
}

enum link_field {
    LF_UNKNOWN,
    LF_ID,
    LF_XATTRS,
    LF_PARENT,
    LF_NAME,
};

static enum link_field
str2link_field(const char *string)
{
    switch (*string++) {
    case 'i': /* id */
        if (strcmp(string, "d"))
            break;
        return LF_ID;
    case 'n': /* name */
        if (strcmp(string, "ame"))
            break;
        return LF_NAME;
    case 'p': /* parent */
        if (strcmp(string, "arent"))
            break;
        return LF_PARENT;
    case 'x': /* xattrs */
        if (strcmp(string, "attrs"))
            break;
        return LF_XATTRS;
    }

    errno = EINVAL;
    return LF_UNKNOWN;
}

static bool
parse_link(yaml_parser_t *parser, struct rbh_fsevent *link)
{
    static struct rbh_id parent;
    struct {
        bool id:1;
        bool parent:1;
        bool name:1;
    } seen = {};

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
            seen.id = true;
            success = parse_id(parser, &link->id);
            break;
        case LF_XATTRS:
            success = parse_rbh_value_map(parser, &link->xattrs);
            break;
        case LF_PARENT:
            seen.parent = true;
            success = parse_id(parser, &parent);
            link->link.parent_id = &parent;
            break;
        case LF_NAME:
            seen.name = true;
            success = parse_name(parser, &link->link.name);
            break;
        }

        if (!success)
            return false;
    }

    return seen.id && seen.parent && seen.name;
}

/*----------------------------------------------------------------------------*
 |                                   unlink                                   |
 *----------------------------------------------------------------------------*/

#define UNLINK_TAG "!unlink"

static bool
emit_unlink(yaml_emitter_t *emitter, const struct rbh_fsevent *unlink)
{
    const struct rbh_id *parent = unlink->link.parent_id;
    const char *name = unlink->link.name;

    return yaml_emit_mapping_start(emitter, UNLINK_TAG)
        && YAML_EMIT_STRING(emitter, "id")
        && yaml_emit_binary(emitter, unlink->id.data, unlink->id.size)
        && YAML_EMIT_STRING(emitter, "parent")
        && yaml_emit_binary(emitter, parent->data, parent->size)
        && YAML_EMIT_STRING(emitter, "name")
        && YAML_EMIT_STRING(emitter, name)
        && yaml_emit_mapping_end(emitter);
}

static bool
parse_unlink(yaml_parser_t *parser, struct rbh_fsevent *unlink)
{
    static struct rbh_id parent;
    struct {
        bool id:1;
        bool parent:1;
        bool name:1;
    } seen = {};

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
            seen.id = true;
            success = parse_id(parser, &unlink->id);
            break;
        case LF_XATTRS:
            /* Skip, xattrs are meaningless for an unlink fsevent */
            if (!yaml_parser_skip_next(parser))
                parser_error(parser);
            success = true;
            break;
        case LF_PARENT:
            seen.parent = true;
            success = parse_id(parser, &parent);
            unlink->link.parent_id = &parent;
            break;
        case LF_NAME:
            seen.name = true;
            success = parse_name(parser, &unlink->link.name);
            break;
        }

        if (!success)
            return false;
    }

    return seen.id && seen.parent && seen.name;
}

/*----------------------------------------------------------------------------*
 |                                   delete                                   |
 *----------------------------------------------------------------------------*/

#define DELETE_TAG "!delete"

static bool
emit_delete(yaml_emitter_t *emitter, const struct rbh_fsevent *delete)
{
    return yaml_emit_mapping_start(emitter, DELETE_TAG)
        && YAML_EMIT_STRING(emitter, "id")
        && yaml_emit_binary(emitter, delete->id.data, delete->id.size)
        && yaml_emit_mapping_end(emitter);
}

static bool
parse_delete(yaml_parser_t *parser, struct rbh_fsevent *delete)
{
    struct {
        bool id:1;
    } seen = {};

    while (true) {
        yaml_event_t event;
        const char *key;
        int save_errno;
        int cmp;

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

        cmp = strcmp(key, "id");
        yaml_event_delete(&event);
        if (cmp) {
            errno = EINVAL;
            return false;
        }

        if (!parse_id(parser, &delete->id))
            return false;

        seen.id = true;
    }

    return seen.id;
}

/*----------------------------------------------------------------------------*
 |                                   xattr                                    |
 *----------------------------------------------------------------------------*/

    /*--------------------------------------------------------------------*
     |                             namespace                              |
     *--------------------------------------------------------------------*/

#define NS_XATTR_TAG "!ns_xattr"

static bool
emit_ns_xattr(yaml_emitter_t *emitter, const struct rbh_fsevent *ns_xattr)
{
    const struct rbh_id *parent = ns_xattr->ns.parent_id;
    const char *name = ns_xattr->ns.name;

    return yaml_emit_mapping_start(emitter, NS_XATTR_TAG)
        && YAML_EMIT_STRING(emitter, "id")
        && yaml_emit_binary(emitter, ns_xattr->id.data, ns_xattr->id.size)
        && YAML_EMIT_STRING(emitter, "xattrs")
        && emit_rbh_value_map(emitter, &ns_xattr->xattrs)
        && YAML_EMIT_STRING(emitter, "parent")
        && yaml_emit_binary(emitter, parent->data, parent->size)
        && YAML_EMIT_STRING(emitter, "name")
        && YAML_EMIT_STRING(emitter, name)
        && yaml_emit_mapping_end(emitter);
}

static bool
parse_ns_xattr(yaml_parser_t *parser, struct rbh_fsevent *ns_xattr)
{
    static struct rbh_id parent;
    struct {
        bool id:1;
        bool parent:1;
        bool name:1;
    } seen = {};

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
            seen.id = true;
            success = parse_id(parser, &ns_xattr->id);
            break;
        case LF_XATTRS:
            success = parse_rbh_value_map(parser, &ns_xattr->xattrs);
            break;
        case LF_PARENT:
            seen.parent = true;
            success = parse_id(parser, &parent);
            ns_xattr->ns.parent_id = &parent;
            break;
        case LF_NAME:
            seen.name = true;
            success = parse_name(parser, &ns_xattr->ns.name);
            break;
        }

        if (!success)
            return false;
    }

    return seen.id && seen.parent && seen.name;
}

    /*--------------------------------------------------------------------*
     |                               inode                                |
     *--------------------------------------------------------------------*/

#define INODE_XATTR_TAG "!inode_xattr"

static bool
emit_inode_xattr(yaml_emitter_t *emitter, const struct rbh_fsevent *inode_xattr)
{
    return yaml_emit_mapping_start(emitter, INODE_XATTR_TAG)
        && YAML_EMIT_STRING(emitter, "id")
        && yaml_emit_binary(emitter, inode_xattr->id.data, inode_xattr->id.size)
        && YAML_EMIT_STRING(emitter, "xattrs")
        && emit_rbh_value_map(emitter, &inode_xattr->xattrs)
        && yaml_emit_mapping_end(emitter);
}

enum inode_xattr_field {
    IXF_UNKNOWN,
    IXF_ID,
    IXF_XATTRS,
};

static enum inode_xattr_field
str2inode_xattr_field(const char *string)
{
    switch (*string++) {
    case 'i': /* id */
        if (strcmp(string, "d"))
            break;
        return IXF_ID;
    case 'x': /* xattrs */
        if (strcmp(string, "attrs"))
            break;
        return IXF_XATTRS;
    }

    errno = EINVAL;
    return IXF_UNKNOWN;
}

static bool
parse_inode_xattr(yaml_parser_t *parser, struct rbh_fsevent *inode_xattr)
{
    struct {
        bool id:1;
    } seen = {};

    while (true) {
        enum inode_xattr_field field;
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

        field = str2inode_xattr_field(key);
        save_errno = errno;
        yaml_event_delete(&event);

        switch (field) {
        case IXF_UNKNOWN:
            errno = save_errno;
            return false;
        case IXF_ID:
            seen.id = true;
            success = parse_id(parser, &inode_xattr->id);
            break;
        case IXF_XATTRS:
            success = parse_rbh_value_map(parser, &inode_xattr->xattrs);
            break;
        }

        if (!success)
            return false;
    }

    return seen.id;
}

/*---------------------------------- xattr -----------------------------------*/

static bool
emit_xattr(yaml_emitter_t *emitter, const struct rbh_fsevent *xattr)
{
    if (xattr->ns.parent_id) {
        assert(xattr->ns.name);
        return emit_ns_xattr(emitter, xattr);
    }
    assert(xattr->ns.name == NULL);

    return emit_inode_xattr(emitter, xattr);
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
