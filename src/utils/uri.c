/* This file is part of the RobinHood Library
 * Copyright (C) 2019 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
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
backend_from_uri(const struct rbh_uri *uri)
{
    const struct rbh_filter_projection ID_ONLY = {
        .fsentry_mask = RBH_FP_ID,
    };
    struct rbh_backend *backend = backend_new(uri->backend, uri->fsname);
    struct rbh_backend *branch = NULL; /* gcc: unitialized variable */
    struct rbh_fsentry *fsentry;
    int save_errno;

    switch (uri->type) {
    case RBH_UT_BARE:
        return backend;
    case RBH_UT_ID:
        branch = rbh_backend_branch(backend, uri->id);
        break;
    case RBH_UT_PATH:
        fsentry = rbh_backend_fsentry_from_path(backend, uri->path, &ID_ONLY);
        if (fsentry == NULL)
            error(EXIT_FAILURE, errno, "rbh_backend_fsentry_from_path");

        if (!(fsentry->mask & RBH_FP_ID))
            error(EXIT_FAILURE, ENODATA, "rbh_backend_fsentry_from_path");

        branch = rbh_backend_branch(backend, &fsentry->id);
        save_errno = errno;
        free(fsentry);
        errno = save_errno;
        break;
    }

    save_errno = errno;
    rbh_backend_destroy(backend);
    errno = save_errno;
    if (branch == NULL)
        error(EXIT_FAILURE, errno, "rbh_backend_branch");

    return branch;
}

struct rbh_backend *
rbh_backend_from_uri(const char *string)
{
    struct rbh_backend *backend;
    struct rbh_raw_uri *raw_uri;
    struct rbh_uri *uri;

    raw_uri = rbh_raw_uri_from_string(string);
    if (raw_uri == NULL)
        error(EXIT_FAILURE, errno, "rbh_raw_uri_from_string");

    uri = rbh_uri_from_raw_uri(raw_uri);
    if (uri == NULL)
        error(EXIT_FAILURE, errno, "rbh_uri_from_raw_uri");
    free(raw_uri);

    backend = backend_from_uri(uri);
    free(uri);
    return backend;
}
