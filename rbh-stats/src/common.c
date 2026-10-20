/* This file is part of RobinHood 4
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <robinhood.h>

#include "stats.h"

struct output_handler {
    enum output_format output_format;
    bool need_comma;
    int tab_count;
};

static void
print_tabs(struct output_handler *oh)
{
    for (int i = 0; i < oh->tab_count; ++i)
        printf("    ");
}

void
print_timespec(const struct rbh_value *value, struct output_handler *oh)
{
    struct timespec timespec;

    assert(value->type == RBH_VT_MAP);
    assert(value->map.count == 2);

    timespec.tv_sec = value->map.pairs[0].value->int64;
    timespec.tv_nsec = value->map.pairs[1].value->int64;

    switch (oh->output_format) {
    case OF_JSON:
        printf("\"%lu.%09lu\"", timespec.tv_sec, timespec.tv_nsec);
        break;
    default:
        printf("%lu.%09lu", timespec.tv_sec, timespec.tv_nsec);
        break;
    }
}

void
print_time_from_timestamp(const struct rbh_value *value,
                          struct output_handler *oh)
{
    time_t time = (time_t) value->int64;

    switch (oh->output_format) {
    case OF_NORMAL:
    case OF_ONELINE:
        printf("%s", time_from_timestamp(&time));
        break;
    case OF_CSV:
        printf("%lu", time);
        break;
    case OF_JSON:
        printf("\"%s\"", time_from_timestamp(&time));
        break;
    }
}

void
print_difftime(const struct rbh_value *value, struct output_handler *oh)
{
    char _buffer[32];
    size_t bufsize;
    char *buffer;

    buffer = _buffer;
    bufsize = sizeof(_buffer);

    difftime_printer(buffer, bufsize, value->int64);

    switch (oh->output_format) {
    case OF_JSON:
        printf("\"%s\"", buffer);
        break;
    default:
        printf("%s", buffer);
        break;
    }
}

void
print_value(const struct rbh_value *value, struct output_handler *oh)
{
    if (oh->output_format == OF_JSON)
        printf("\"");

    switch (value->type) {
    case RBH_VT_STRING:
        if (oh->output_format == OF_CSV)
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
    case RBH_VT_MAP:
        printf("enotsup");
        break;
    default:
        error(EXIT_FAILURE, EINVAL, "Unexpected key type to print '%d'",
              value->type);
        __builtin_unreachable();
    }

    if (oh->output_format == OF_JSON)
        printf("\"");
}

static int
key2common_stats_value(const char *key)
{
    switch (key[0]) {
    case 'c':
        if (!strcmp(&key[1], "ommand_line"))
            return CSV_COMMAND_LINE;

        break;
    case 'd':
        if (!strcmp(&key[1], "uration"))
            return CSV_DURATION;

        break;
    case 'e':
        if (!strcmp(&key[1], "nd_time"))
            return CSV_END_TIME;

        break;
    case 's':
        if (!strcmp(&key[1], "tart_time"))
            return CSV_START_TIME;

        break;
    }

    return CSV_UNKNOWN;
}

static const struct formatted_stats_value common_formatted_stats_value[] = {
    [CSV_START_TIME] =    { .header = "Start of the command",
                            .oneline_header = "Start",
                            .print_stats_value = print_time_from_timestamp,
                            .oneline = true },
    [CSV_DURATION] =      { .header = "Duration of the command",
                            .oneline_header = "Duration",
                            .print_stats_value = print_difftime,
                            .oneline = true },
    [CSV_END_TIME] =      { .header = "End of the command",
                            .print_stats_value = print_time_from_timestamp },
    [CSV_COMMAND_LINE] =  { .header = "Command used",
                            .print_stats_value = print_value },
};

static void
print_stats_info(const struct rbh_value *value,
                 const struct formatted_stats_value *flv,
                 struct output_handler *oh)
{
    if (oh->output_format == OF_ONELINE && !flv->oneline)
        return;

    if (oh->need_comma) {
        switch (oh->output_format) {
        case OF_NORMAL:
            break;
        case OF_ONELINE:
            printf(", ");
            break;
        case OF_CSV:
            printf(",");
            break;
        case OF_JSON:
            printf(",\n");
            break;
        }
    }

    switch (oh->output_format) {
    case OF_NORMAL:
        print_tabs(oh);
        printf("- %-*s: ", WIDTH, flv->header);
        flv->print_stats_value(value, oh);
        printf("\n");
        break;
    case OF_ONELINE:
        printf("%s: ", flv->oneline_header);
        flv->print_stats_value(value, oh);
        oh->need_comma = true;
        break;
    case OF_CSV:
        flv->print_stats_value(value, oh);
        oh->need_comma = true;
        break;
    case OF_JSON:
        print_tabs(oh);
        printf("\"%s\": ", flv->header);
        flv->print_stats_value(value, oh);
        oh->need_comma = true;
        break;
    }
}

void
print_stats_wrapper(const struct rbh_value_map *stats,
                    enum output_format output_format,
                    const struct formatted_stats_value *flv,
                    int (*key2stats_value)(const char *))
{
    struct output_handler oh = {
        .output_format = output_format,
        .need_comma = false,
        .tab_count = 1
    };

    for (size_t i = 0 ; i < stats->count ; i++) {
        const struct rbh_value_pair *pair = &stats->pairs[i];
        enum common_stats_value common_stats_value;

        common_stats_value = key2common_stats_value(pair->key);
        if (common_stats_value != CSV_UNKNOWN) {
            print_stats_info(pair->value,
                             &common_formatted_stats_value[common_stats_value],
                             &oh);
            continue;
        }

        if (flv == NULL)
            continue;

        /* key2stats_value could theoretically return a int outside of the
         * array, but the 'key2<command>_stats_value' functions either give a
         * valid index or simply error out. So there is no possible segfault
         * here.
         */
        print_stats_info(pair->value, &flv[key2stats_value(pair->key)], &oh);
    }
}
