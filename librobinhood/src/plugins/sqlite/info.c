/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include "internals.h"

int
sqlite_backend_insert_source(void *backend,
                             const struct rbh_value *backend_source)
{
    return 0;
}

static struct rbh_value POSIX_STRING = {
    .type = RBH_VT_STRING,
    .string = "posix",
};

static struct rbh_value LUSTRE_STRING = {
    .type = RBH_VT_STRING,
    .string = "lustre",
};

static struct rbh_value RETENTION_STRING = {
    .type = RBH_VT_STRING,
    .string = "retention",
};

static struct rbh_value EXTENSION_STRING = {
    .type = RBH_VT_STRING,
    .string = "extension",
};

static struct rbh_value PLUGIN_STRING = {
    .type = RBH_VT_STRING,
    .string = "plugin",
};

static const struct rbh_value_pair POSIX_MAP_VALUES[2] = {
    { .key = "type", .value = &PLUGIN_STRING },
    { .key = "plugin", .value = &POSIX_STRING },
};

static const struct rbh_value_pair LUSTRE_MAP_VALUES[3] = {
    { .key = "type", .value = &EXTENSION_STRING },
    { .key = "plugin", .value = &POSIX_STRING },
    { .key = "extension", .value = &LUSTRE_STRING },
};

static const struct rbh_value_pair RETENTION_MAP_VALUES[3] = {
    { .key = "type", .value = &EXTENSION_STRING },
    { .key = "plugin", .value = &POSIX_STRING },
    { .key = "extension", .value = &RETENTION_STRING },
};

static struct rbh_value VALUES[3] = {
    {
        .type = RBH_VT_MAP,
        .map = {
            .count = 2,
            .pairs = POSIX_MAP_VALUES,
        }
    },
    {
        .type = RBH_VT_MAP,
        .map = {
            .count = 3,
            .pairs = LUSTRE_MAP_VALUES,
        }
    },
    {
        .type = RBH_VT_MAP,
        .map = {
            .count = 3,
            .pairs = RETENTION_MAP_VALUES,
        }
    },
};

static struct rbh_value POSIX_VALUE = {
    .type = RBH_VT_SEQUENCE,
    .sequence = {
        .count = 3,
        .values = VALUES,
    },
};

static struct rbh_value_pair POSIX_BACKEND = {
    .key = "backend_source",
    .value = &POSIX_VALUE,
};

static struct rbh_value_map POSIX_SOURCE = {
    .count = 1,
    .pairs = &POSIX_BACKEND,
};

struct rbh_value_map *
sqlite_backend_get_info(void *backend, int flags)
{
    if (flags & RBH_INFO_BACKEND_SOURCE)
        return &POSIX_SOURCE;

    return NULL;
}
