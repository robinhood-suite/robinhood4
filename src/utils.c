/* This file is part of rbh-find
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "rbh-find/utils.h"

char *
shell2pcre(const char *shell)
{
    char *pcre;
    size_t len = 0, i = 0, j = 0, k = 0;
    bool escaped = false, end = false;

    /* Compute the size of the destination string */
    while (!end) {
        switch (shell[i++]) {
            case '\0':
                end = true;
                break;
            case '\\':
                escaped = !escaped;
                break;
            case '*':
            case '?':
            case '|':
            case '+':
            case '(':
            case ')':
            case '{':
            case '}':
                if (!escaped)
                    len++;
                escaped = false;
                break;
            case '[':
            case ']':
                escaped = false;
                break;
            default:
                if (escaped)
                    len--;
                escaped = false;
                break;
        }
        len++;
    }

    /*                                     `len'
     *                                    vvvvvvv
     * Allocate the destination string: "^<regex>(?!\n)$"
     *                                   ^       ^^^^^^^^
     *                                   1       234 5678
     */
    pcre = malloc(len + 8);
    if (pcre == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    len = i - 1; /* len = strlen(shell) */
    pcre[k++] = '^';
    escaped = false;
    for (i = 0; i < len; i++) {
        switch (shell[i]) {
        case '\\':
            escaped = !escaped;
            break;
        case '*':
            if (!escaped) {
                strncpy(&pcre[k], &shell[j], i - j);
                k += i - j;
                j = i;
                pcre[k++] = '.';
            }
            escaped = false;
            break;
        case '?':
            if (!escaped) {
                strncpy(&pcre[k], &shell[j], i - j);
                k += i - j;
                j = i + 1;
                pcre[k++] = '.';
            }
            escaped = false;
            break;
        case '.':
        case '|':
        case '+':
        case '(':
        case ')':
        case '{':
        case '}':
            if (!escaped) {
                /* Those characters need to be escaped */
                strncpy(&pcre[k], &shell[j], i - j);
                k += i - j;
                j = i;
                pcre[k++] = '\\';
            }
            escaped = false;
            break;
        case '[':
        case ']':
            escaped = false;
            break;
        default:
            if (escaped) {
                /* Skip the escape char, it is meaningless */
                strncpy(&pcre[k], &shell[j], i - j - 1);
                k += i - j - 1;
                j = i;
            }
            escaped = false;
            break;
        }
    }
    strncpy(&pcre[k], &shell[j], i - j);
    k += i - j;
    snprintf(&pcre[k], 7, "(?!\n)$");

    return pcre;
}

const unsigned long TIME_UNIT2SECONDS[] = {
    [TU_SECOND] = 1,
    [TU_MINUTE] = 60,
    [TU_HOUR] = 3600,
    [TU_DAY] = 86400,
};

unsigned long
str2seconds(enum time_unit unit, const char *string)
{
    unsigned long delta;
    char *endptr;

    delta = strtoul(string, &endptr, 10);
    if ((errno == ERANGE && delta == ULONG_MAX) || (errno != 0 && delta == 0))
        return delta;
    if (*endptr != '\0') {
        errno = EINVAL;
        return 0;
    }

    if (ULONG_MAX / TIME_UNIT2SECONDS[unit] < delta) {
        errno = ERANGE;
        return ULONG_MAX;
    }

    return delta * TIME_UNIT2SECONDS[unit];
}

int
str2uint64_t(const char *input, uint64_t *result)
{
    char *end;

    errno = 0;
    *result = strtoull(input, &end, 0);
    if (errno) {
        return -1;
    } else if ((!*result && input == end) || *end != '\0') {
        errno = EINVAL;
        return -1;
    }

    return 0;
}
