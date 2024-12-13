/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <sys/stat.h>

#include <robinhood.h>

#include "report.h"

int
dump_value(const struct rbh_value *value, char *buffer)
{
    char *safekeep = buffer;

    switch (value->type) {
    case RBH_VT_INT32:
        return sprintf(buffer, "%d", value->int32);
    case RBH_VT_INT64:
        return sprintf(buffer, "%ld", value->int64);
    case RBH_VT_STRING:
        return sprintf(buffer, "%s", value->string);
    case RBH_VT_SEQUENCE:
        buffer += sprintf(buffer, "[");
        for (int j = 0; j < value->sequence.count; j++) {
            buffer += dump_value(&value->sequence.values[j], buffer);
            if (j < value->sequence.count - 1)
                buffer += sprintf(buffer, "; ");
        }
        buffer += sprintf(buffer, "]");
        return buffer - safekeep;
    default:
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "Unexpected value type, found '%s'",
                      VALUE_TYPE_NAMES[value->type]);
    }

    __builtin_unreachable();
}

static int
dump_type_value(const struct rbh_value *value, char *buffer)
{
    if (value->type != RBH_VT_INT32)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "Unexpected value type, expected 'int32', found '%s'",
                      VALUE_TYPE_NAMES[value->type]);

    switch (value->int32) {
    case S_IFBLK:
        return sprintf(buffer, "block");
    case S_IFCHR:
        return sprintf(buffer, "char");
    case S_IFDIR:
        return sprintf(buffer, "directory");
    case S_IFREG:
        return sprintf(buffer, "file");
    case S_IFLNK:
        return sprintf(buffer, "link");
    case S_IFIFO:
        return sprintf(buffer, "fifo");
    case S_IFSOCK:
        return sprintf(buffer, "socket");
    default:
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__,
                      "unexpected file type '%d'", value->type);
    }

    __builtin_unreachable();
}

int
dump_decorated_value(const struct rbh_value *value,
                     const struct rbh_filter_field *field,
                     char *buffer)
{
    switch (field->fsentry) {
    case RBH_FP_STATX:
        if (field->statx == RBH_STATX_TYPE)
            return dump_type_value(value, buffer);
    default:
        return dump_value(value, buffer);
    }

    __builtin_unreachable();
}

void
dump_results(const struct rbh_value_map *result_map,
             const struct rbh_group_fields group,
             const struct rbh_filter_output output,
             struct result_columns *columns)
{
    const struct rbh_value_map *output_map;
    const struct rbh_value_map *id_map;

    if (result_map->count == 2) {
        id_map = &result_map->pairs[0].value->map;
        output_map = &result_map->pairs[1].value->map;
    } else {
        id_map = NULL;
        output_map = &result_map->pairs[0].value->map;
    }

    if (id_map) {
        dump_id_map(id_map, group);
        printf(": ");
    }

    dump_output_map(output_map, output);
}

void
init_id_columns(struct result_columns *columns, int id_count)
{
    columns->id_count = id_count;

    if (id_count == 0) {
        columns->id_columns = NULL;
    } else {
        columns->id_columns = rbh_sstack_push(
            values_sstack, NULL,
            columns->id_count * sizeof(*(columns->id_columns)));
        if (columns->id_columns == NULL)
            error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_sstack_push");
    }
}

void
init_output_columns(struct result_columns *columns, int output_count)
{
    columns->output_count = output_count;

    columns->output_columns = rbh_sstack_push(
        values_sstack, NULL,
        columns->output_count * sizeof(*(columns->output_columns)));
    if (columns->output_columns == NULL)
        error_at_line(EXIT_FAILURE, errno, __FILE__, __LINE__,
                      "rbh_sstack_push");
}

void
init_column(struct column *column, const char *string)
{
    column->header = strdup(string);
    if (column->header == NULL)
        error_at_line(EXIT_FAILURE, EINVAL, __FILE__, __LINE__, "strdup");

    column->length = strlen(string);
}
>>>>>>> 2faf32d (report: add `pretty-print` option)
