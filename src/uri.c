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

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "robinhood/uri.h"

#include "lu_fid.h"

/*----------------------------------------------------------------------------*
 |                         rbh_raw_uri_from_string()                          |
 *----------------------------------------------------------------------------*/

/* URI generic syntax: scheme:[//authority]path[?query][#fragment]
 *
 * where authority is: [userinfo@]host[:port]
 *
 * where userinfo is: username[:password]
 *
 * cf. RFC 3986 for more information
 */

struct rbh_raw_uri *
rbh_raw_uri_from_string(const char *string_)
{
    char *pound, *qmark, *slash, *at, *colon;
    struct rbh_raw_uri *raw_uri;
    char *string;
    size_t size;

    if (!isalpha(*string_)) {
        errno = EINVAL;
        return NULL;
    }

    size = strlen(string_) + 1;
    raw_uri = malloc(sizeof(*raw_uri) + size);
    if (raw_uri == NULL)
        return NULL;
    memset(raw_uri, 0, sizeof(*raw_uri));
    string = (char *)raw_uri + sizeof(*raw_uri);
    memcpy(string, string_, size);

    raw_uri->scheme = string;
    do {
        string++;
    } while (isalnum(*string) || *string == '+' || *string == '-'
            || *string == '.');

    if (*string != ':') {
        free(raw_uri);
        errno = EINVAL;
        return NULL;
    }
    *string++ = '\0';

    /* string = [//authority]path[?query][#fragment] */
    pound = strrchr(string, '#');
    if (pound) {
        *pound++ = '\0';
        raw_uri->fragment = pound;
    }

    /* string = [//authority]path[?query] */
    qmark = strrchr(string, '?');
    if (qmark) {
        *qmark++ = '\0';
        raw_uri->query = qmark;
    }

    /* string = [//authority]path */
    if (string[0] != '/' || string[1] != '/') {
        /* string = path */
        raw_uri->path = string;
        return raw_uri;
    }

    /* string = //[userinfo@]host[:port]path
     *
     * where path is either empty or starts with a '/'
     */
    slash = strchr(string + 2, '/');
    if (slash) {
        raw_uri->path = slash;
        /* Up until now, there always was a separator to overwrite with a '\0'.
         * Here, we cannot overwrite '/' as it is part of the path and we need
         * to keep it.
         *
         * Fortunately, there are two leading '/' in `string' we do not care
         * about.
         */
        memmove(string, string + 2, slash - string - 2);
        memset(slash - 2, '\0', 2);
    } else {
        raw_uri->path = "";
        string += 2;
    }

    /* string = [userinfo@]host[:port] */
    at = strchr(string, '@');
    if (at) {
        /* string = userinfo@host[:port] */
        raw_uri->userinfo = string;
        *at++ = '\0';
        string = at;
    }

    /* string = host[:port] */
    colon = strrchr(string, ':');
    if (colon) {
        /* string = host:port */
        *colon++ = '\0';
        raw_uri->port = colon;
    }

    /* string = host */
    raw_uri->host = string;

    return raw_uri;
}

/*----------------------------------------------------------------------------*
 |                            rbh_percent_decode()                            |
 *----------------------------------------------------------------------------*/

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
percent_decode(int major, int minor)
{
    return (major << 4) + minor;
}

ssize_t
rbh_percent_decode(char *dest, const char *src, size_t n)
{
    size_t count = 0;

    while (*src != '\0' && n-- > 0) {
        int major, minor;

        count++;
        if (*src != '%') {
            *dest++ = *src++;
            continue;
        }
        /* Discard the '%' */
        src++;

        /* There must be at least 2 characters left to parse */
        if (n < 2) {
            errno = EILSEQ;
            return -1;
        }
        n -= 2;

        major = hex2int(*src++);
        if (major < 0) {
            if (errno == EINVAL)
                errno = EILSEQ;
            return -1;
        }

        minor = hex2int(*src++);
        if (minor < 0) {
            if (errno == EINVAL)
                errno = EILSEQ;
            return -1;
        }

        *dest++ = percent_decode(major, minor);
    }

    return count;
}

/*----------------------------------------------------------------------------*
 |                           rbh_uri_from_raw_uri()                           |
 *----------------------------------------------------------------------------*/

static struct rbh_id *
id_from_fid_string(const char *fid_string, size_t length)
{
    struct lu_fid fid;
    char *end;

    if (lu_fid_init_from_string(fid_string, &fid, &end))
        return NULL;

    if (fid_string + length != end) {
        errno = EINVAL;
        return NULL;
    }

    return rbh_id_from_lu_fid(&fid);
}

static struct rbh_id *
id_from_encoded_fid_string(const char *encoded, size_t length)
{
    struct rbh_id *id = NULL;
    int save_errno;
    char *decoded;
    ssize_t size;

    decoded = malloc(length + 1);
    if (decoded == NULL)
        return NULL;

    size = rbh_percent_decode(decoded, encoded, length);
    if (size < 0)
        goto out_free_decoded;
    decoded[size] = '\0';

    id = id_from_fid_string(decoded, size);
out_free_decoded:
    save_errno = errno;
    free(decoded);
    errno = save_errno;
    return id;
}

static struct rbh_id *
id_from_encoded_string(const char *encoded_string, size_t length)
{
    struct rbh_id *id;
    ssize_t size;
    char *data;

    id = malloc(sizeof(*id) + length);
    if (id == NULL)
        return NULL;
    data = (char *)id + sizeof(*id);

    id->data = data;
    size = rbh_percent_decode(data, encoded_string, length);
    if (size < 0) {
        int save_errno = errno;

        free(id);
        errno = save_errno;
        return NULL;
    }

    id->size = size;
    return id;
}

static struct rbh_id *
id_from_fragment(const char *fragment)
{
    const char *colon;
    size_t length;

    if (*fragment++ != '[') {
        errno = EINVAL;
        return NULL;
    }

    length = strlen(fragment);
    if (fragment[length - 1] != ']') {
        errno = EINVAL;
        return NULL;
    }
    /* Discard the trailing ']' */
    length--;

    colon = strchr(fragment, ':');
    /* Is this a Lustre FID? */
    if (colon) {
        /* It must be */
        if (strchr(colon + 1, ':') == NULL) {
            /* Oho, missing a second ':', are we? */
            errno = EINVAL;
            return NULL;
        }
        return id_from_encoded_fid_string(fragment, length);
    }

    return id_from_encoded_string(fragment, length);
}

struct rbh_uri *
rbh_uri_from_raw_uri(const struct rbh_raw_uri *raw_uri)
{
    struct rbh_id *id = NULL;
    struct rbh_uri *uri;
    const char *colon;
    int save_errno;
    size_t size;
    char *data;
    ssize_t rc;

    if (strcmp(raw_uri->scheme, RBH_SCHEME)) {
        errno = EINVAL;
        return NULL;
    }

    colon = strchr(raw_uri->path, ':');
    if (colon == NULL) {
        errno = EINVAL;
        return NULL;
    }

    size = strlen(raw_uri->path) + 1;
    if (raw_uri->fragment) {
        id = id_from_fragment(raw_uri->fragment);
        if (id == NULL)
            return NULL;
        size += sizeof(*id) + id->size;
    }

    uri = malloc(sizeof(*uri) + size);
    if (uri == NULL) {
        save_errno = errno;
        goto out_free_id;
    }
    data = (char *)uri + sizeof(*uri);

    /* uri->backend */
    uri->backend = data;
    rc = rbh_percent_decode(data, raw_uri->path, colon - raw_uri->path);
    if (rc < 0) {
        save_errno = errno;
        goto out_free_uri;
    }

    assert(rc < size);
    data[rc] = '\0';
    data += rc + 1;
    size -= rc + 1;

    /* uri->fsname */
    uri->fsname = data;
    rc = rbh_percent_decode(data, colon + 1, -1);
    if (rc < 0) {
        save_errno = errno;
        goto out_free_uri;
    }

    assert(rc < size);
    data[rc] = '\0';
    data += rc + 1;
    size -= rc + 1;

    /* uri->id */
    if (id) {
        void *tmp = data;
        int rc;

        uri->id = tmp;
        data += sizeof(*uri->id);
        size -= sizeof(*uri->id);

        rc = rbh_id_copy(tmp, id, &data, &size);
        assert(rc == 0);
        free(id);
    } else {
        uri->id = NULL;
    }

    return uri;

out_free_uri:
    free(uri);
out_free_id:
    free(id);
    errno = save_errno;
    /* The size of the buffer is computed to be an exact fit */
    assert(errno != ENOBUFS);
    return NULL;
}
