/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <sys/stat.h>

#include "robinhood/value.h"
#include "robinhood/statx.h"

#include "mongo.h"

    /*--------------------------------------------------------------------*
     |                         bson_iter_count()                          |
     *--------------------------------------------------------------------*/

static bool
bson_type_is_supported(bson_type_t type)
{
    switch (type) {
    case BSON_TYPE_UTF8:
    case BSON_TYPE_DOCUMENT:
    case BSON_TYPE_ARRAY:
    case BSON_TYPE_BINARY:
    case BSON_TYPE_BOOL:
    case BSON_TYPE_NULL:
    case BSON_TYPE_INT32:
    case BSON_TYPE_INT64:
    case BSON_TYPE_DOUBLE:
        return true;
    case BSON_TYPE_REGEX:
        /* TODO: support it */
    default:
        return false;
    }
}

/* XXX: the iterator will be consumed, libbson lacks a practical
 *      bson_iter_copy() / bson_iter_tee() / bson_iter_dup()
 */
size_t
bson_iter_count(bson_iter_t *iter)
{
    size_t count = 0;

    while (bson_iter_next(iter)) {
        if (bson_type_is_supported(bson_iter_type(iter)))
            count++;
    }

    return count;
}

void
escape_field_path(char *field_path)
{
    for (int i = 0; i < strlen(field_path); i++)
        if (field_path[i] == '.')
            field_path[i] = '_';
}

void
dump_bson(bson_t *to_dump)
{
    char *dump_str;

    if (!to_dump)
        return;

    dump_str = bson_as_canonical_extended_json(to_dump, NULL);
    fprintf(stderr, "Dumped bson = '%s'\n", dump_str);
    free(dump_str);
}
