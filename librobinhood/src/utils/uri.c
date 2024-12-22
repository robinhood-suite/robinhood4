/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "value.h"
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <dlfcn.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "robinhood/plugins/backend.h"
#include "robinhood/utils.h"
#include "robinhood/uri.h"


#include <stdio.h>
#include <limits.h>

static const struct rbh_backend_plugin *
backend_plugin_import(const char *name)
{
    const struct rbh_backend_plugin *plugin;
    int save_errno = errno;

    errno = 0;
    plugin = rbh_backend_plugin_import(name);
    if (plugin == NULL) {
        if (errno == 0)
            error(EXIT_FAILURE, 0, "Unable to load backend plugin: %s",
                  dlerror());
        error(EXIT_FAILURE, errno, "Unable to load backend plugin");
    }

    errno = save_errno;
    return plugin;
}

static char *
config_extends_key(const char *backend)
{
    char *key;

    if (asprintf(&key, "backends/%s/extends", backend) == -1)
        error(EXIT_FAILURE, ENOMEM, "Failed to build extends key");

    return key;
}

static const char *
resolve_config_plugin_name(struct rbh_config *config, const char *backend)
{
    enum key_parse_result rc;
    struct rbh_value plugin;
    char *key;

    if (!config)
        return backend;

    key = config_extends_key(backend);
    rc = rbh_config_find(key, &plugin, RBH_VT_STRING);
    free(key);
    switch (rc) {
        case KPR_NOT_FOUND:
            return backend;
        case KPR_FOUND:
            return plugin.string;
        default:
            error(EXIT_FAILURE, errno, "Failed to retrieve plugin from config");
    }
    __builtin_unreachable();
}

static struct rbh_backend *
backend_new(const char *type, const char *fsname)
{
    const struct rbh_backend_plugin *plugin;
    struct rbh_backend *backend;
    struct rbh_config *config;

    config = get_rbh_config();
    plugin = backend_plugin_import(resolve_config_plugin_name(config, type));

    backend = rbh_backend_plugin_new(plugin, fsname, config);
    if (backend == NULL)
        error(EXIT_FAILURE, errno, "rbh_backend_plugin_new");

    return backend;
}

static struct rbh_backend *
posix_backend_branch_from_path(struct rbh_backend *backend, const char *fsname,
                               const char *path)
{
    char full_path[PATH_MAX];

    /* Discard every leading '/' in `path' */
    while (*path == '/')
        path++;

    if (fsname[0] != '/') {
        if (getcwd(full_path, sizeof(full_path)) == NULL)
            error(EXIT_FAILURE, errno, "getcwd");

        if (snprintf(full_path + strlen(full_path),
                     PATH_MAX - strlen(full_path),
                     "/%s/%s", fsname, path) == -1)
            error(EXIT_FAILURE, errno, "snprintf with '%s', '%s' and '%s'",
                  full_path, fsname, path);
    } else {
        if (snprintf(full_path, PATH_MAX, "%s/%s", fsname, path) == -1)
            error(EXIT_FAILURE, errno, "snprintf with '%s' and '%s'",
                  fsname, path);
    }

    return rbh_backend_branch(backend, NULL, full_path);
}

static struct rbh_backend *
backend_branch_from_path(struct rbh_backend *backend, const char *path)
{
    const struct rbh_filter_projection ID_ONLY = {
        .fsentry_mask = RBH_FP_ID,
    };
    struct rbh_fsentry *fsentry;
    struct rbh_backend *branch;
    int save_errno;

    fsentry = rbh_backend_fsentry_from_path(backend, path, &ID_ONLY);
    if (fsentry == NULL)
        error(EXIT_FAILURE, errno, "rbh_backend_fsentry_from_path");

    if (!(fsentry->mask & RBH_FP_ID))
        error(EXIT_FAILURE, ENODATA, "rbh_backend_fsentry_from_path");

    branch = rbh_backend_branch(backend, &fsentry->id, NULL);
    save_errno = errno;
    free(fsentry);
    errno = save_errno;
    return branch;
}

static struct rbh_backend *
backend_from_uri(const struct rbh_uri *uri)
{
    struct rbh_backend *backend = backend_new(uri->backend, uri->fsname);
    struct rbh_backend *branch = NULL; /* gcc: unitialized variable */
    int save_errno;

    switch (uri->type) {
    case RBH_UT_BARE:
        return backend;
    case RBH_UT_ID:
        branch = rbh_backend_branch(backend, uri->id, NULL);
        break;
    case RBH_UT_PATH:
        /* The posix/posix-mpi and lustre/lustre-mpi backend do not support
         * filtering, treat it differently */
        if (backend->id == RBH_BI_POSIX || backend->id == RBH_BI_POSIX_MPI ||
            backend->id == RBH_BI_LUSTRE || backend->id == RBH_BI_LUSTRE_MPI)
            branch = posix_backend_branch_from_path(backend, uri->fsname,
                                                    uri->path);
        else
            branch = backend_branch_from_path(backend, uri->path);
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
        error(EXIT_FAILURE, errno, "Cannot detect backend uri");

    uri = rbh_uri_from_raw_uri(raw_uri);
    if (uri == NULL)
        error(EXIT_FAILURE, errno, "Cannot detect given backend");
    free(raw_uri);

    backend = backend_from_uri(uri);
    free(uri);
    return backend;
}

// vim: expandtab:ts=4:sw=4
