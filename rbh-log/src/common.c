/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "log.h"

void
print_time_from_timestamp(const struct rbh_value *value, const char *header)
{
    time_t time = (time_t) value->int64;

    printf(" - %-*s: %s\n", WIDTH, header, time_from_timestamp(&time));
}

void
print_difftime(const struct rbh_value *value, const char *header)
{
    char _buffer[32];
    size_t bufsize;
    char *buffer;

    buffer = _buffer;
    bufsize = sizeof(_buffer);

    difftime_printer(buffer, bufsize, value->int64);

    printf(" - %-*s: %s\n", WIDTH, header, buffer);
}

void
print_value(const struct rbh_value *value, const char *header)
{
    switch (value->type) {
    case RBH_VT_STRING:
        printf(" - %-*s: %s\n", WIDTH, header, value->string);
        break;
    case RBH_VT_INT64:
        printf(" - %-*s: %ld\n", WIDTH, header, value->int64);
        break;
    default:
        error(EXIT_FAILURE, EINVAL, "Unexpected key type to print '%s': %d",
              header, value->type);
        __builtin_unreachable();
    }
}
