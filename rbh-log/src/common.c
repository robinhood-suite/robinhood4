/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "log.h"

void
print_timespec(const struct rbh_value *value, const char *header,
               enum output_format output_format)
{
    struct timespec timespec;

    assert(value->type == RBH_VT_MAP);
    assert(value->map.count == 2);

    timespec.tv_sec = value->map.pairs[0].value->int64;
    timespec.tv_nsec = value->map.pairs[1].value->int64;

    switch (output_format) {
    case OF_NORMAL:
        printf(" - %-*s: %lu.%09lu\n", WIDTH, header,
               timespec.tv_sec, timespec.tv_nsec);
        break;
    case OF_ONELINE:
        printf("%s: %lu.%09lu", header,
               timespec.tv_sec, timespec.tv_nsec);
        break;
    case OF_CSV:
        printf("%lu.%09lu", timespec.tv_sec, timespec.tv_nsec);
        break;
    case OF_JSON:
        printf("        \"%s\": \"%lu.%09lu\"", header,
               timespec.tv_sec, timespec.tv_nsec);
        break;
    }
}

void
print_time_from_timestamp(const struct rbh_value *value, const char *header,
                          enum output_format output_format)
{
    time_t time = (time_t) value->int64;

    switch (output_format) {
    case OF_NORMAL:
        printf(" - %-*s: %s\n", WIDTH, header, time_from_timestamp(&time));
        break;
    case OF_ONELINE:
        printf("%s: %s", header, time_from_timestamp(&time));
        break;
    case OF_CSV:
        printf("%s", time_from_timestamp(&time));
        break;
    case OF_JSON:
        printf("        \"%s\": \"%s\"", header, time_from_timestamp(&time));
        break;
    }
}

void
print_difftime(const struct rbh_value *value, const char *header,
               enum output_format output_format)
{
    char _buffer[32];
    size_t bufsize;
    char *buffer;

    buffer = _buffer;
    bufsize = sizeof(_buffer);

    difftime_printer(buffer, bufsize, value->int64);

    switch (output_format) {
    case OF_NORMAL:
        printf(" - %-*s: %s\n", WIDTH, header, buffer);
        break;
    case OF_ONELINE:
        printf("%s: %s", header, buffer);
        break;
    case OF_CSV:
        printf("%s", buffer);
        break;
    case OF_JSON:
        printf("        \"%s\": \"%s\"", header, buffer);
        break;
    }
}

void
print_value(const struct rbh_value *value, const char *header,
            enum output_format output_format)
{
    switch (output_format) {
    case OF_NORMAL:
        printf(" - %-*s: ", WIDTH, header);
        break;
    case OF_ONELINE:
        printf("%s: ", header);
        break;
    case OF_CSV:
        break;
    case OF_JSON:
        printf("        \"%s\": \"", header);
        break;
    }

    switch (value->type) {
    case RBH_VT_STRING:
        if (output_format == OF_CSV)
            printf("\"%s\"", value->string);
        else
            printf("%s", value->string);

        break;
    case RBH_VT_INT64:
        printf("%ld", value->int64);
        break;
    case RBH_VT_DOUBLE:
        /* .3f will round the value, which can make other tools harder to use
         * as most of them truncate the result if we print only a few digits of
         * the decimal. So to make it more exact, we instead truncate the double
         * by first multiplying it to have the number of decimals we want,
         * convert to int to truncate, then divide again to get the truncated
         * double.
         */
        printf("%.3f", ((int) (10000 * value->float64)) / 10000.0);
        break;
    default:
        error(EXIT_FAILURE, EINVAL, "Unexpected key type to print '%s': %d",
              header, value->type);
        __builtin_unreachable();
    }

    switch (output_format) {
    case OF_NORMAL:
        printf("\n");
        break;
    case OF_JSON:
        printf("\"");
        break;
    default:
        break;
    }
}

static int
key2common_log_value(const char *key)
{
    switch (key[0]) {
    case 'c':
        if (!strcmp(&key[1], "ommand_line"))
            return CLV_COMMAND_LINE;

        break;
    case 'd':
        if (!strcmp(&key[1], "uration"))
            return CLV_DURATION;

        break;
    case 'e':
        if (!strcmp(&key[1], "nd_time"))
            return CLV_END_TIME;

        break;
    case 's':
        if (!strcmp(&key[1], "tart_time"))
            return CLV_START_TIME;

        break;
    }

    return CLV_UNKNOWN;
}

static const struct formatted_log_value common_formatted_log_value[] = {
    [CLV_START_TIME] =    { .header = "Start of the command",
                            .oneline_header = "Start",
                            .print_log_value = print_time_from_timestamp,
                            .oneline = true },
    [CLV_DURATION] =      { .header = "Duration of the command",
                            .oneline_header = "Duration",
                            .print_log_value = print_difftime,
                            .oneline = true },
    [CLV_END_TIME] =      { .header = "End of the command",
                            .print_log_value = print_time_from_timestamp },
    [CLV_COMMAND_LINE] =  { .header = "Command used",
                            .print_log_value = print_value },
};

static void
print_log_info(const struct rbh_value *value,
               const struct formatted_log_value *flv,
               enum output_format output_format,
               bool *need_comma)
{
    switch (output_format) {
    case OF_NORMAL:
        flv->print_log_value(value, flv->header, output_format);
        break;
    case OF_ONELINE:
        if (!flv->oneline)
            break;

        if (*need_comma)
            printf(", ");

        flv->print_log_value(value, flv->oneline_header, output_format);
        *need_comma = true;
        break;
    case OF_CSV:
        if (*need_comma)
            printf(",");

        flv->print_log_value(value, flv->header, output_format);
        *need_comma = true;
        break;
    case OF_JSON:
        if (*need_comma)
            printf(",\n");

        flv->print_log_value(value, flv->header, output_format);
        *need_comma = true;
        break;
    }
}

void
print_log_wrapper(const struct rbh_value_map *log,
                  enum output_format output_format,
                  const struct formatted_log_value *flv,
                  int (*key2log_value)(const char *))
{
    bool need_comma = false;

    for (size_t i = 0 ; i < log->count ; i++) {
        const struct rbh_value_pair *pair = &log->pairs[i];
        enum common_log_value common_log_value;

        common_log_value = key2common_log_value(pair->key);
        if (common_log_value != CLV_UNKNOWN) {
            print_log_info(pair->value,
                           &common_formatted_log_value[common_log_value],
                           output_format,
                           &need_comma);
            continue;
        }

        if (flv == NULL)
            continue;

        /* key2log_value could theoretically return a int outside of the array,
         * but the 'key2<command>_log_value' functions either give a valid index
         * or simply error out. So there is no possible segfault here.
         */
        print_log_info(pair->value, &flv[key2log_value(pair->key)],
                       output_format, &need_comma);
    }
}
