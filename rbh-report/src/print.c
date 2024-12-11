/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <sys/stat.h>

#include <robinhood.h>

#include "report.h"

static void
dump_value(const struct rbh_value *value)
{
    switch (value->type) {
    case RBH_VT_INT32:
        printf("%d", value->int32);
        break;
    case RBH_VT_INT64:
        printf("%ld", value->int64);
        break;
    case RBH_VT_STRING:
        printf("%s", value->string);
        break;
    case RBH_VT_SEQUENCE:
        printf("[");
        for (int j = 0; j < value->sequence.count; j++) {
            dump_value(&value->sequence.values[j]);
            if (j < value->sequence.count - 1)
                printf("; ");
        }
        printf("]");
        break;
    default:
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "Unexpected value type, found '%s'",
                      VALUE_TYPE_NAMES[value->type]);
    }
}

static void
dump_type_value(const struct rbh_value *value)
{
    if (value->type != RBH_VT_INT32)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "Unexpected value type, expected 'int32', found '%s'",
                      VALUE_TYPE_NAMES[value->type]);

    switch (value->int32) {
    case S_IFBLK:
        printf("block");
        break;
    case S_IFCHR:
        printf("char");
        break;
    case S_IFDIR:
        printf("directory");
        break;
    case S_IFREG:
        printf("file");
        break;
    case S_IFLNK:
        printf("link");
        break;
    case S_IFIFO:
        printf("fifo");
        break;
    case S_IFSOCK:
        printf("socket");
        break;
    default:
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "unexpected file type '%d'", value->int32);
    }
}

static void
dump_decorated_value(const struct rbh_value *value,
                     const struct rbh_filter_field *field)
{
    switch (field->fsentry) {
    case RBH_FP_STATX:
        if (field->statx == RBH_STATX_TYPE) {
            dump_type_value(value);
            return;
        }
    default:
        dump_value(value);
    }
}

void
dump_id_map(const struct rbh_value_map *map, struct rbh_group_fields group)
{
    if (map->count != group.id_count)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "Unexpected number of fields in id map, expected '%ld', got '%ld'",
                      group.id_count, map->count);

    for (int i = 0; i < map->count; i++) {
        dump_decorated_value(map->pairs[i].value, &group.id_fields[i].field);

        if (i < map->count - 1)
            printf(",");
    }
}

void
dump_output_map(const struct rbh_value_map *map,
                const struct rbh_filter_output output)
{
    if (map->count != output.output_fields.count)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "Unexpected number of fields in output map, expected '%ld', got '%ld'",
                      output.output_fields.count, map->count);

    for (int i = 0; i < map->count; i++) {
        dump_value(map->pairs[i].value);

        if (i < map->count - 1)
            printf(",");
    }
}
