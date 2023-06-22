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
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static struct file_handle *
path2handle_at(int dirfd, const char *path, int flags)
{
    struct file_handle *handle;
    int mount_id;

    handle = malloc(MAX_HANDLE_SZ);
    if (handle == NULL)
        error(EXIT_FAILURE, errno, "malloc");
    handle->handle_bytes = MAX_HANDLE_SZ - sizeof(*handle);

    while (name_to_handle_at(dirfd, path, handle, &mount_id, flags)) {
        if (errno != EOVERFLOW)
            error(EXIT_FAILURE, errno, "name_to_handle_at: %s", path);

        handle = realloc(handle, sizeof(*handle) + handle->handle_bytes);
        if (handle == NULL)
            error(EXIT_FAILURE, errno, "realloc");
    }

    return handle;
}

static struct rbh_id *
path2id_at(int dirfd, const char *path, int flags)
{
    struct file_handle *handle;
    struct rbh_id *id;

    handle = path2handle_at(dirfd, path, flags);

    id = rbh_id_from_file_handle(handle);
    if (id == NULL)
        error(EXIT_FAILURE, errno, "rbh_id_from_file_handle");

    free(handle);
    return id;
}

static struct rbh_backend *
posix_backend_branch_from_path(struct rbh_backend *backend, const char *fsname,
                               const char *path)
{
    struct rbh_backend *branch;
    struct rbh_id *id;
    int save_errno;
    int dirfd;

    dirfd = open(fsname, O_RDONLY);
    if (dirfd < 0)
        error(EXIT_FAILURE, errno, "open: %s", fsname);

    /* Discard every leading '/' in `path' */
    while (*path == '/')
        path++;

    /* AT_EMPTY_PATH is required as `path' may be empty */
    id = path2id_at(dirfd, path, AT_EMPTY_PATH);
    /* Ignore errors on close */
    close(dirfd);

    branch = rbh_backend_branch(backend, id);
    save_errno = errno;
    free(id);
    errno = save_errno;
    return branch;
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

    branch = rbh_backend_branch(backend, &fsentry->id);
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
        branch = rbh_backend_branch(backend, uri->id);
        break;
    case RBH_UT_PATH:
        /* The posix backend does not support filtering, treat it differently */
        if (backend->id == RBH_BI_POSIX)
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
        error(EXIT_FAILURE, errno, "rbh_raw_uri_from_string");

    uri = rbh_uri_from_raw_uri(raw_uri);
    if (uri == NULL)
        error(EXIT_FAILURE, errno, "rbh_uri_from_raw_uri");
    free(raw_uri);

    backend = backend_from_uri(uri);
    free(uri);
    return backend;
}
