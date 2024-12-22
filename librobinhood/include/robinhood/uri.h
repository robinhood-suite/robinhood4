/* This file is part of RobinHood 4
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_URI_H
#define ROBINHOOD_URI_H

#include <sys/types.h>

#include "robinhood/fsentry.h"
#include "robinhood/uri.h"

#define RBH_SCHEME "rbh"
#define RBH_SOURCE "src"

/*----------------------------------------------------------------------------*
 |                                rbh_raw_uri                                 |
 *----------------------------------------------------------------------------*/

struct rbh_raw_uri {
    const char *scheme; /* Non-nullable */
    const char *userinfo;
    const char *host;
    const char *port;
    const char *path; /* Non-nullable */
    const char *query;
    const char *fragment;
};

/**
 * Parse a URI into a struct rbh_raw_uri
 *
 * @param string    the URI string to parse
 *
 * @return          a pointer to a newly allocated struct rbh_raw_uri on
 *                  success, NULL on error and errno is set appropriately
 *
 * @error EINVAL    \p string is not a valid URI according to RFC3986.
 * @error ENOMEM    not enough memory available
 *
 * Note that this function does not perform any check other than those necessary
 * to split a URI into a struct rbh_raw_uri.
 */
struct rbh_raw_uri *
rbh_raw_uri_from_string(const char *string);

/**
 * Test if the given string is a valid URI
 *
 * @param string    the URI string to check
 *
 * @return          a true if \p string is a valid URI, false otherwise
 */
bool
rbh_is_uri(const char *string);

/*----------------------------------------------------------------------------*
 |                                rbh_raw_uri                                 |
 *----------------------------------------------------------------------------*/

enum rbh_uri_type {
    RBH_UT_BARE,
    RBH_UT_ID,
    RBH_UT_PATH,
};

struct rbh_uri {
    enum rbh_uri_type type;
    const char *backend;
    const char *fsname;
    union {
        /* RBH_UT_ID */
        const struct rbh_id *id;

        /* RBH_UT_PATH */
        const char *path;
    };
};

/**
 * Create a new struct rbh_uri from a struct rbh_raw_uri
 *
 * @param raw_uri   a pointer to the struct rbh_raw_uri to interpret into a
 *                  struct rbh_uri
 *
 * @return          a pointer to a newly allocated struct rbh_uri on success,
 *                  NULL on error and errno is set appropriately
 *
 * @error EILSEQ    \p raw_uri contains misencoded data
 * @error EINVAL    \p raw_uri is not a valid rbh URI
 * @error ENOMEM    not enough memory available
 */
struct rbh_uri *
rbh_uri_from_raw_uri(const struct rbh_raw_uri *raw_uri);

/*----------------------------------------------------------------------------*
 |                             rbh_percent_decode                             |
 *----------------------------------------------------------------------------*/

/**
 * Decode a percent-encoded string
 *
 * @param dest      a pointer to a buffer big enough to contain the decoded \n
 *                  first bytes of src
 * @param src       the string to decode
 * @param n         the number of bytes from \p src to decode
 *
 * @return          the number of bytes written to \p dest on success, -1 on
 *                  error and errno is set appropriately.
 *
 * @error EILSEQ    \p src contains misencoded data
 *
 * \p dest and \p src may overlap.
 *
 * At most \p n bytes from \p src will be decoded. The decoded data will
 * necessarily be as long or shorter than the encoded data ("%xy" --> 'z').
 *
 * Note that the decoded data is not necessarily a string. Therefore it will not
 * be null-terminated (unless the encoded data was terminated by an encoded
 * null-terminating byte). Feel free to add a null-terminating byte yourself:
 *
 *     char test[] = "abc%64efg%68ijk%6cmno";
 *     ssize_t size;
 *
 *     size = rbh_percent_decode(test, test, sizeof(test));
 *     assert(size < sizeof(test));
 *     test[size] = '\0';
 *     assert(strcmp(test, "abcdefghijklmno") == 0);
 */
ssize_t
rbh_percent_decode(char *dest, const char *src, size_t n);

#endif
