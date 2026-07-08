/* This file is part of RobinHood
 * Copyright (C) 2026 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include <assert.h>
#include <errno.h>
#include <string.h>
#include "range_parser.h"
#include "robinhood/utils.h"


/* Parse a prefixed number in the form <prefix><number>. */
static int
parse_prefixed_number(const char *string, char prefix, int max, uint64_t *number)
{
    if (*string != prefix) {
        errno =  EINVAL;
        return -1;
    }

    if (str2uint64_t(string + 1, number)) {
        errno = EINVAL;
        return -1;
    }

    if (*number > max) {
        errno = EINVAL;
        return -1;
    }

    return 0;
}


static int
append_interval(struct selinux_level *level, int32_t first, int32_t last)
{
    struct selinux_interval *previous;

    if (last < first) {
        errno = EINVAL;
        return -1;
    }

    if (level->intervals_count > 0) {
        previous = &level->intervals[level->intervals_count - 1];
        /*
         * Merge the new interval with the previous one when they are adjacent.
         */
        if (first >= previous->first && first <= previous->last + 1) {
            if (last > previous->last)
                previous->last = last;

            return 0;
        }

        if (first <= previous->last) {
            errno = EINVAL;
            return -1;
        }
    }

    level->intervals[level->intervals_count].first = first;
    level->intervals[level->intervals_count].last = last;
    level->intervals_count++;

    return 0;
}

/**
 * Parse either one category or one category interval.
 */
static int
parse_category_item(char *string, struct selinux_level *level)
{
    uint64_t first;
    uint64_t last;
    char *dot;

    dot = strchr(string, '.');
    if (dot)
        *dot = '\0';

    if (parse_prefixed_number(string, 'c', SELINUX_MAX_CATEGORY, &first))
        return -1;

    if (!dot) {
        last = first;
    } else {
        if (parse_prefixed_number(dot + 1, 'c', SELINUX_MAX_CATEGORY, &last))
            return -1;
    }

    return append_interval(level, first, last);
}


/**
 * Parse a category separated by a comma and append its intervals to the level.
 */
static int
parse_category_set(char *string, struct selinux_level *level)
{
    char *comma;

    if (*string == '\0')
        return 0;

    for (;;) {
        comma = strchr(string, ',');
        if (comma)
            *comma = '\0';

        if (parse_category_item(string, level))
            return -1;

        if (!comma)
            return 0;

        string = comma + 1;
    }
}


int
selinux_parse_level(char *string, struct selinux_level *level)
{
    uint64_t sens;
    char *colon;

    level->sens = 0;
    level->intervals_count = 0;

    colon = strchr(string, ':');
    if (colon)
        *colon = '\0';

    if (parse_prefixed_number(string, 's', SELINUX_MAX_SENSITIVITY, &sens))
        return -1;

    level->sens = sens;

    if (!colon)
        return 0;

    string = colon + 1;

    if (*string != 'c') {
        errno = EINVAL;
        return -1;
    }

    return parse_category_set(string, level);
}


/**
 * Parse a simple SELinux level or a low-high range.
 */
int
selinux_parse_range(char *range, struct selinux_range *parsed)
{
    char *dash;

    dash = strchr(range, '-');
    if (dash)
        *dash = '\0';

    if (selinux_parse_level(range, &parsed->low))
        return -1;

    if (dash) {
        if (selinux_parse_level(dash + 1, &parsed->high))
            return -1;
    } else {
        parsed->high.sens = parsed->low.sens;
        parsed->high.intervals_count = parsed->low.intervals_count;

        memcpy(parsed->high.intervals, parsed->low.intervals,
               parsed->low.intervals_count * sizeof(*parsed->low.intervals));
    }

    return 0;
}


int
split_selinux_context(char *ctx, struct selinux_context *parts)
{
    char *sep1;
    char *sep2;
    char *sep3;

    sep1 = strchr(ctx, ':');
    if (sep1 == NULL)
        goto invalid;

    sep2 = strchr(sep1 + 1, ':');
    if (sep2 == NULL)
        goto invalid;

    sep3 = strchr(sep2 + 1, ':');
    if (sep3 == NULL)
        goto invalid;

    if (sep1 == ctx || sep2 == sep1 + 1 ||
        sep3 == sep2 + 1 || sep3[1] == '\0')
        goto invalid;

    *sep1 = '\0';
    *sep2 = '\0';
    *sep3 = '\0';

    parts->user = ctx;
    parts->role = sep1 + 1;
    parts->type = sep2 + 1;
    parts->range = sep3 + 1;

    return 0;

invalid:
    errno = EINVAL;
    return -1;
}
