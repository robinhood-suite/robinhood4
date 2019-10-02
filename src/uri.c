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
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

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
    const size_t length = strlen(string);
    size_t count = 0;

    for (char *c = string; *c != '\0'; c++) {
        int major, minor;

        if (*c != '%')
            continue;

        major = hex2int(*(c + 1));
        if (major < 0) {
            if (errno == EINVAL)
                errno = EILSEQ;
            return -1;
        }

        minor = hex2int(*(c + 2));
        if (minor < 0) {
            if (errno == EINVAL)
                errno = EILSEQ;
            return -1;
        }

        *c = _percent_decode(major, minor);
        memmove(c + 1, c + 3, length - (c - string) - 2 * ++count);
    }

    return count;
}

static uint64_t
strtou64(const char *nptr, char **endptr, int base)
{
#if ULLONG_MAX == UINT64_MAX
    return strtoull(nptr, endptr, base);
#else /* ULLONG_MAX > UINT64_MAX */
    unsigned long long value;
    int save_errno = errno;

    errno = 0;
    value = strtoull(nptr, endptr, base);
    if (value == ULLONG_MAX && errno != 0)
        return UINT64_MAX;

    if (value > UINT64_MAX) {
        errno = ERANGE;
        return UINT64_MAX;
    }

    errno = save_errno;
    return value;
#endif
}

static uint32_t
strtou32(const char *nptr, char **endptr, int base)
{
#if ULONG_MAX == UINT32_MAX
    return strtoul(nptr, endptr, base);
#else /* ULONG_MAX > UINT32_MAX */
    unsigned long value;
    int save_errno = errno;

    errno = 0;
    value = strtoul(nptr, endptr, base);
    if (value == ULONG_MAX && errno != 0)
        return UINT32_MAX;

    if (value > UINT32_MAX) {
        errno = ERANGE;
        return UINT32_MAX;
    }

    errno = save_errno;
    return value;
#endif
}

struct lu_fid {
    uint64_t f_seq;
    uint32_t f_oid;
    uint32_t f_ver;
};

static int
_parse_fid(struct rbh_uri *uri, const struct lu_fid *fid)
{
    const size_t LUSTRE_FH_SIZE = sizeof(int) + 2 * sizeof(*fid);
    const int FILEID_LUSTRE = 0x97;

    if (sizeof(uri->buffer) < LUSTRE_FH_SIZE) {
        errno = ENOBUFS;
        return -1;
    }

    memcpy(uri->buffer, &FILEID_LUSTRE, sizeof(int));
    memcpy(uri->buffer + sizeof(int), fid, sizeof(*fid));
    memset(uri->buffer + sizeof(int) + sizeof(*fid), 0, sizeof(*fid));

    uri->id.data = uri->buffer;
    uri->id.size = LUSTRE_FH_SIZE;

    return 0;
}

static int
parse_fid(struct rbh_uri *uri, char *sequence, char *oid, char *version)
{
    int save_errno = errno;
    struct lu_fid fid;
    char *nul;

    /* sequence (uint64_t) */
    if (percent_decode(sequence) < 0)
        return -1;

    errno = 0;
    fid.f_seq = strtou64(sequence, &nul, 0);
    if (fid.f_seq == UINT64_MAX && errno != 0)
        return -1;
    if (*nul != '\0') {
        errno = EINVAL;
        return -1;
    }

    /* oid (uint32_t) */
    if (percent_decode(oid) < 0)
        return -1;

    errno = 0;
    fid.f_oid = strtou32(oid, &nul, 0);
    if (fid.f_oid == UINT32_MAX && errno != 0)
        return -1;
    if (*nul != '\0') {
        errno = EINVAL;
        return -1;
    }

    /* version (uint32_t) */
    if (percent_decode(version) < 0)
        return -1;

    errno = 0;
    fid.f_ver = strtou32(version, &nul, 0);
    if (fid.f_ver == UINT32_MAX && errno != 0)
        return -1;
    if (*nul != '\0') {
        errno = EINVAL;
        return -1;
    }

    errno = save_errno;
    return _parse_fid(uri, &fid);
}

static int
parse_fragment(struct rbh_uri *uri, char *fragment)
{
    size_t length;
    ssize_t count;
    char *colon;

    if (*fragment++ != '[') {
        errno = EINVAL;
        return -1;
    }

    length = strlen(fragment);
    if (fragment[length - 1] != ']') {
        errno = EINVAL;
        return -1;
    }
    fragment[--length] = '\0';

    /* Is this a FID? */
    colon = strchr(fragment, ':');
    if (colon) { /* Yes */
        char *sequence = fragment;
        char *oid = colon + 1;
        char *version;

        *colon = '\0';

        version = strchr(colon + 1, ':');
        if (!version) {
            errno = EINVAL;
            return -1;
        }
        *version++ = '\0';

        return parse_fid(uri, sequence, oid, version);
    } /* No */

    count = percent_decode(fragment);
    if (count < 0)
        return -1;

    uri->id.data = fragment;
    uri->id.size = length - 2 * (count);
    /*                    ^^^^^^^^^^^^^
     *                    for every percent encoded character
     */

    return 0;
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
        return 0;
    }

    return parse_fragment(uri, raw_uri->fragment);
}
