/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <stdio.h>

#include "robinhood/utils.h"
#include "robinhood/value.h"
#include "robinhood/sstack.h"

static int
convert_pair_with_op(struct rbh_value_pair *pair, const char *op,
                     struct rbh_sstack *stack)
{
    const struct rbh_value *xattr_value = pair->value;
    struct rbh_value_pair *pair_op;
    struct rbh_value *value;

    pair_op = RBH_SSTACK_PUSH(stack, NULL, sizeof(*pair_op));
    pair_op->key = op;
    pair_op->value = xattr_value;

    value = RBH_SSTACK_PUSH(stack, NULL, sizeof(*value));
    value->type = RBH_VT_MAP;
    value->map.count = 1;
    value->map.pairs = pair_op;

    pair->value = value;

    return 0;
}

int
convert_xattrs_with_operation(const struct rbh_value_pair *pairs, int count,
                              const char *op, struct rbh_sstack *stack)
{
    for (int i = 0; i < count; i++) {
        struct rbh_value_pair *pair = (struct rbh_value_pair *) &pairs[i];

        if (convert_pair_with_op(pair, op, stack))
            return -1;
    }

    return 0;
}

/* Helper to print rbh_value in Python-like format */
static void
rbh_value_print(FILE *out, const struct rbh_value *value)
{
    if (!value) {
        fprintf(out, "null");
        return;
    }

    switch (value->type) {
    case RBH_VT_BOOLEAN:
        fprintf(out, "%s", value->boolean ? "True" : "False");
        break;
    case RBH_VT_INT32:
        fprintf(out, "%d", value->int32);
        break;
    case RBH_VT_UINT32:
        fprintf(out, "%u", value->uint32);
        break;
    case RBH_VT_INT64:
        fprintf(out, "%"PRId64, value->int64);
        break;
    case RBH_VT_UINT64:
        fprintf(out, "%"PRIu64, value->uint64);
        break;
    case RBH_VT_STRING:
        fprintf(out, "\"%s\"", value->string);
        break;
    case RBH_VT_BINARY:
        fprintf(out, "<binary %zu bytes>", value->binary.size);
        break;
    case RBH_VT_REGEX:
        fprintf(out, "<regex>");
        break;
    case RBH_VT_SEQUENCE:
        fprintf(out, "[");
        for (size_t i = 0; i < value->sequence.count; i++) {
            if (i > 0) fprintf(out, ", ");
            rbh_value_print(out, &value->sequence.values[i]);
        }
        fprintf(out, "]");
        break;
    case RBH_VT_MAP: {
        fprintf(out, "{");
        for (size_t i = 0; i < value->map.count; i++) {
            if (i > 0) fprintf(out, ", ");
            fprintf(out, "\"%s\": ", value->map.pairs[i].key);
            rbh_value_print(out, value->map.pairs[i].value);
        }
        fprintf(out, "}");
        break;
    }
    case RBH_VT_NULL:
        fprintf(out, "null");
        break;
    default:
        fprintf(out, "<unknown>");
        break;
    }
}

/* Format value_map as Python-like dict string */
char *
rbh_value_map_to_string(const struct rbh_value_map *map)
{
    char *result = NULL;
    size_t size = 0;
    FILE *stream;

    stream = open_memstream(&result, &size);
    if (!stream)
        return NULL;

    fprintf(stream, "{");
    for (size_t i = 0; i < map->count; i++) {
        if (i > 0) fprintf(stream, ", ");
        fprintf(stream, "\"%s\": ", map->pairs[i].key);
        rbh_value_print(stream, map->pairs[i].value);
    }
    fprintf(stream, "}");

    fclose(stream);
    return result;
}
