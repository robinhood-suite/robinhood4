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

#include <dlfcn.h>
#include <errno.h>
#include <error.h>
#include <stdlib.h>
#include <string.h>

#include "robinhood/plugins/backend.h"
#include "robinhood/utils.h"
#include "robinhood/uri.h"

static const struct rbh_backend_plugin *
backend_plugin_import(const char *name)
{
    const struct rbh_backend_plugin *plugin;
    int save_errno = errno;

    errno = 0;
    plugin = rbh_backend_plugin_import(name);
    if (plugin == NULL) {
        if (errno == 0)
            error(EXIT_FAILURE, 0, "rbh_backend_plugin_import: %s", dlerror());
        error(EXIT_FAILURE, errno, "rbh_backend_plugin_import");
    }

    errno = save_errno;
    return plugin;
}

static struct rbh_backend *
backend_new(const char *type, const char *fsname)
{
    const struct rbh_backend_plugin *plugin = backend_plugin_import(type);
    struct rbh_backend *backend;

    backend = rbh_backend_plugin_new(plugin, fsname);
    if (backend == NULL)
        error(EXIT_FAILURE, errno, "rbh_backend_plugin_new");

    return backend;
}

static struct rbh_backend *
_backend_from_uri(const struct rbh_uri *uri, char *path)
{
    struct rbh_backend *backend = backend_new(uri->backend, uri->fsname);
    struct rbh_backend *branch;
    int save_errno;

    if (path != NULL) {
        struct rbh_fsentry *fsentry;

        fsentry = rbh_backend_fsentry_from_path(backend, path, RBH_FP_ID, 0);
        if (fsentry == NULL)
            error(EXIT_FAILURE, errno, "rbh_backend_fsentry_from_path");

        if (!(fsentry->mask & RBH_FP_ID))
            error(EXIT_FAILURE, ENODATA, "rbh_backend_fsentry_from_path");

        branch = rbh_backend_branch(backend, &fsentry->id);
        save_errno = errno;
        free(fsentry);
        errno = save_errno;
    } else if (uri->id) {
        branch = rbh_backend_branch(backend, uri->id);
    } else {
        return backend;
    }

    save_errno = errno;
    rbh_backend_destroy(backend);
    errno = save_errno;
    if (branch == NULL)
        error(EXIT_FAILURE, errno, "rbh_backend_branch");

    return branch;
}

static struct rbh_backend *
backend_from_uri(const struct rbh_uri *uri, const char *encoded_path)
{
    struct rbh_backend *backend = NULL;
    char *decoded_path;
    size_t length;
    ssize_t rc;

    if (encoded_path == NULL)
        return _backend_from_uri(uri, NULL);

    length = strlen(encoded_path);
    decoded_path = malloc(length + 1);
    if (decoded_path == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    rc = rbh_percent_decode(decoded_path, encoded_path, length);
    if (rc < 0)
        error(EXIT_FAILURE, errno, "rbh_percent_decode");
    decoded_path[rc] = '\0';

    backend = _backend_from_uri(uri, decoded_path);
    free(decoded_path);
    return backend;
}

static struct rbh_backend *
backend_from_raw_uri(const struct rbh_raw_uri *raw_uri,
                     const char *encoded_path)
{
    struct rbh_backend *backend;
    struct rbh_uri *uri;

    uri = rbh_uri_from_raw_uri(raw_uri);
    if (uri == NULL)
        error(EXIT_FAILURE, errno, "rbh_parse_uri");

    backend = backend_from_uri(uri, encoded_path);
    free(uri);
    return backend;
}

struct rbh_backend *
rbh_backend_from_uri(const char *string)
{
    const char *encoded_path = NULL;
    struct rbh_backend *backend;
    struct rbh_raw_uri *raw_uri;

    raw_uri = rbh_raw_uri_from_string(string);
    if (raw_uri == NULL)
        error(EXIT_FAILURE, errno, "rbh_parse_raw_uri");

    if (raw_uri->fragment && raw_uri->fragment[0] != '[') {
        encoded_path = raw_uri->fragment;
        raw_uri->fragment = NULL;
    }

    backend = backend_from_raw_uri(raw_uri, encoded_path);
    free(raw_uri);
    return backend;
}
