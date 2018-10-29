/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "utils.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

    /* Allocate the destination string */
    pcre = malloc(len + 7);
    if (pcre == NULL) {
        errno = ENOMEM;
        return NULL;
    }

    /* len == strlen(shell) + 1 */
    len = i;
    pcre[k++] = '^';
    escaped = false;
    for (i = 0; i < len - 1; i++) {
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
