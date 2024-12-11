/* This file is part of Robinhood 4
 * Copyright (C) 2024 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>

#include <robinhood.h>
#include <robinhood/backend.h>
#include <robinhood/uri.h>

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

int
dump_decorated_value(const struct rbh_value *value,
                     const struct rbh_filter_field *field,
                     char *buffer)
{
    switch (field->fsentry) {
    default:
        return dump_value(value, buffer);
    }

    __builtin_unreachable();
}

