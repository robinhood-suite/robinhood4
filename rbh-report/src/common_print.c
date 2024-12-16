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
