/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 *
 * author: Quentin Bouget <quentin.bouget@cea.fr>
 */

#ifndef ROBINHOOD_URI_H
#define ROBINHOOD_URI_H

#include <robinhood/fsentry.h>

#define RBH_SCHEME "rbh"

struct rbh_raw_uri {
    char *scheme;
    char *userinfo;
    char *host;
    char *port;
    char *path;
    char *query;
    char *fragment;
};

/**
 * Parse a URI into a struct rbh_raw_uri
 *
 * @param raw_uri   a pointer to a struct rbh_raw_uri
 * @param string    the URI string to parse (will be modified and should not
 *                  be accessed directly aymore)
 *
 * @return          0 on success, -1 on error and errno is set appropriately
 *
 * @error EINVAL    \p string is not a valid URI according to RFC 3986.
 *
 * Note that this function does not perform any check other than those necessary
 * to splitting a URI into a struct rbh_raw_uri.
 */
int
rbh_parse_raw_uri(struct rbh_raw_uri *raw_uri, char *string);

struct rbh_uri {
    char *backend;
    char *fsname;
    struct rbh_id id;
};

/**
 * Parse a struct rbh_raw_uri into a struct rbh_uri
 *
 * @param uri       a pointer to a struct rbh_uri
 * @param raw_uri   the raw URI to parse into \p uri (will be modified and
 *                  should not be accessed directly anymore).
 *
 * @return          0 on success, -1 on error and errno is set appriopriately
 *
 * @error EINVAL    either \p raw_uri is not a valid robinhood uri, or it is
 *                  incorrectly encoded.
 */
int
rbh_parse_uri(struct rbh_uri *uri, struct rbh_raw_uri *raw_uri);

#endif
