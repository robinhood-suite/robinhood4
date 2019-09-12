/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "robinhood/uri.h"

/* URI generic syntax: scheme:[//authority]path[?query][#fragment]
 *
 * where authority is: [userinfo@]host[:port]
 *
 * where userinfo is: username[:password]
 *
 * cf. RFC 3986 for more information
 */

int
rbh_parse_raw_uri(struct rbh_raw_uri *uri, char *string)
{
    char *at;

    /* string = scheme:[[//authority]path[?query][#fragment]
     *
     * where scheme = ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )
     */
    if (!isalpha(*string)) {
        errno = EINVAL;
        return -1;
    }

    memset(uri, 0, sizeof(*uri));

    uri->scheme = string;
    do {
        string++;
    } while (isalnum(*string) || *string == '+' || *string == '-'
                || *string == '.');

    if (*string != ':') {
        errno = EINVAL;
        return -1;
    }
    *string++ = '\0';

    /* string = [//authority]path[?query][#fragment] */
    uri->fragment = strrchr(string, '#');
    if (uri->fragment)
        *uri->fragment++ = '\0';

    /* string = [//authority]path[?query] */
    uri->query = strrchr(string, '?');
    if (uri->query)
        *uri->query++ = '\0';

    /* string = [//authority]path */
    if (string[0] != '/' || string[1] != '/') {
        /* string = path */
        uri->path = string;
        return 0;
    }

    /* string = //[userinfo@]host[:port]path
     *
     * where path is either empty or starts with a '/'
     */
    uri->path = strchrnul(string + 2, '/');
    if (uri->path == string + 2)
        /* authority is empty */
        return 0;

    /* Move everything to the left of path, two chars to the left
     * (overwriting the leading "//") so [userinfo@]host[:port] can be
     * separated from path with a '\0' (actually 2 of them).
     */
    memmove(string, string + 2, uri->path - string - 2);
    memset((char *)uri->path - 2, '\0', 2);

    /* string = [userinfo@]host[:port] */
    at = strchr(string, '@');
    if (at) {
        /* string = userinfo@host[:port] */
        uri->userinfo = string;
        *at++ = '\0';
        string = at;
    }

    /* string = host[:port] */
    uri->port = strrchr(string, ':');
    if (uri->port)
        *uri->port++ = '\0';

    /* string = host */
    uri->host = *string == '\0' ? NULL : string;

    return 0;
}

static int
hex2int(char c)
{
    switch (c) {
    case '0':
        return 0;
    case '1':
        return 1;
    case '2':
        return 2;
    case '3':
        return 3;
    case '4':
        return 4;
    case '5':
        return 5;
    case '6':
        return 6;
    case '7':
        return 7;
    case '8':
        return 8;
    case '9':
        return 9;
    case 'a':
    case 'A':
        return 10;
    case 'b':
    case 'B':
        return 11;
    case 'c':
    case 'C':
        return 12;
    case 'd':
    case 'D':
        return 13;
    case 'e':
    case 'E':
        return 14;
    case 'f':
    case 'F':
        return 15;
    }
    errno = EINVAL;
    return -1;
}

static char
_percent_decode(int major, int minor)
{
    return (major << 4) + minor;
}

static ssize_t
percent_decode(char *string)
{
    size_t count = 0;

    for (char *c = string; *c != '\0'; c++) {
        int major, minor;

        if (*c != '%')
            continue;
        count++;

        major = hex2int(*(c + 1));
        if (major < 0)
            return -1;

        minor = hex2int(*(c + 2));
        if (minor < 0)
            return -1;

        *c = _percent_decode(major, minor);
        memmove(c + 1, c + 3, strlen(c + 3) + 1);
    }

    return count;
}

int
rbh_parse_uri(struct rbh_uri *uri, struct rbh_raw_uri *raw_uri)
{
    char *colon;

    if (raw_uri->scheme == NULL || strcmp(raw_uri->scheme, RBH_SCHEME)) {
        errno = EINVAL;
        return -1;
    }

    colon = strchr(raw_uri->path, ':');
    if (colon == NULL) {
        errno = EINVAL;
        return -1;
    }
    *colon++ = '\0';

    if (percent_decode(raw_uri->path) < 0)
        return -1;
    uri->backend = raw_uri->path;

    if (percent_decode(colon) < 0)
        return -1;
    uri->fsname = colon;

    if (raw_uri->fragment == NULL) {
        uri->id.data = NULL;
        uri->id.size = 0;
    } else if (raw_uri->fragment[0] == '[') {
        size_t length = strlen(raw_uri->fragment);
        ssize_t count;

        if (raw_uri->fragment[length - 1] != ']') {
            errno = EINVAL;
            return -1;
        }
        raw_uri->fragment[length - 1] = '\0';

        count = percent_decode(raw_uri->fragment + 1);
        if (count < 0)
            return -1;

        uri->id.data = raw_uri->fragment + 1;
        /*                    for the opening and closing brackets
         *                    vvv
         */
        uri->id.size = length - 2 - 2 * (count);
        /*                        ^^^^^^^^^^^^^
         *                        for every percent encoded character
         */
    } else {
        errno = EINVAL;
        return -1;
    }

    return 0;
}
