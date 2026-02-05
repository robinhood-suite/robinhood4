/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

static const struct rbh_backend_operations LDISKFS_BACKEND_OPS = {
    .filter = ldiskfs_backend_filter,
    .get_info = ldiskfs_backend_get_info,
    .destroy = ldiskfs_backend_destroy,
};

static const struct rbh_backend LDISKFS_BACKEND = {
    .id   = RBH_BI_LDISKFS,
    .name = RBH_LDISKFS_BACKEND_NAME,
    .ops  = &LDISKFS_BACKEND_OPS,
};

struct rbh_backend *
rbh_ldiskfs_backend_new(const struct rbh_backend_plugin *self,
                        const struct rbh_uri *uri,
                        struct rbh_config *config,
                        bool read_only)
{
    struct ldiskfs_backend *ldiskfs;
    char *io_opts = NULL;
    errcode_t rc;

    ldiskfs = xcalloc(1, sizeof(*ldiskfs));
    ldiskfs->backend = LDISKFS_BACKEND;

    rc = ext2fs_open2(uri->fsname, io_opts, EXT2_FLAG_SOFTSUPP_FEATURES, 0, 0,
                      unix_io_manager, &ldiskfs->fs);
    if (rc) {
        ldiskfs_error("failed to open device '%s': %s",
                      uri->fsname, strerror(errno));
        free(ldiskfs);
        return NULL;
    }

    ldiskfs->dcache = rbh_dcache_new();
    if (!ldiskfs->dcache) {
        int save_errno = errno;

        ext2fs_close(ldiskfs->fs);
        free(ldiskfs);
        errno = save_errno;
        return NULL;
    }

    return &ldiskfs->backend;
}

void
ldiskfs_backend_destroy(void *backend)
{
    struct ldiskfs_backend *ldiskfs = backend;

    rbh_dcache_destroy(ldiskfs->dcache);
    ext2fs_close(ldiskfs->fs);
    free(ldiskfs);
}

const struct rbh_value plugin_string = {
    .type = RBH_VT_STRING,
    .string = "plugin",
};

const struct rbh_value extension_string = {
    .type = RBH_VT_STRING,
    .string = "extension",
};

const struct rbh_value lustre_string = {
    .type = RBH_VT_STRING,
    .string = "lustre",
};

const struct rbh_value posix_string = {
    .type = RBH_VT_STRING,
    .string = "posix",
};

const struct rbh_value_pair posix_backend_pairs[2] = {
    {
        .key = "type",
        .value = &plugin_string,
    },
    {
        .key = "plugin",
        .value = &posix_string,
    }
};

const struct rbh_value_pair lustre_extension_pairs[3] = {
    {
        .key = "type",
        .value = &extension_string,
    },
    {
        .key = "plugin",
        .value = &posix_string,
    },
    {
        .key = "extension",
        .value = &lustre_string,
    }
};

const struct rbh_value_map posix_backend_map = {
    .count = 2,
    .pairs = posix_backend_pairs,
};

const struct rbh_value_map lustre_extension_map = {
    .count = 3,
    .pairs = lustre_extension_pairs,
};

const struct rbh_value plugin_values[2] = {
    {
        .type = RBH_VT_MAP,
        .map = posix_backend_map,
    },
    {
        .type = RBH_VT_MAP,
        .map = lustre_extension_map,
    }
};

const struct rbh_value sequence = {
    .type = RBH_VT_SEQUENCE,
    .sequence = {
        .count = 2,
        .values = plugin_values,
    },
};

const struct rbh_value_pair sequence_pair = {
    .key = "backend_source",
    .value = &sequence,
};

const struct rbh_value_map info = {
    .count = 1,
    .pairs = &sequence_pair,
};

struct rbh_value_map *
ldiskfs_backend_get_info(void *backend, int info_flags)
{
    if (info_flags & RBH_INFO_BACKEND_SOURCE)
        return (struct rbh_value_map *)&info;

    errno = ENOTSUP;
    return NULL;
}
