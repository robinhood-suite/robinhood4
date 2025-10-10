/* This file is part of Robinhood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#include <errno.h>
#include <error.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "robinhood/backend.h"
#include "robinhood/backends/lustre.h"
#include <robinhood/backends/posix_extension.h>
#include "robinhood/filter.h"
#include "robinhood/statx.h"
#include "robinhood/value.h"

#include "undelete.h"

static char *
get_mountpoint_from_source(struct rbh_backend *source)
{
    struct rbh_value_map *mountpoint_value_map;
    char *mountpoint;

    mountpoint_value_map = rbh_backend_get_info(source,
                                                RBH_INFO_MOUNTPOINT);
    if (mountpoint_value_map == NULL || mountpoint_value_map->count != 1) {
        fprintf(stderr, "Failed to get mountpoint from source URI\n");
        return NULL;
    }

    mountpoint = strdup(mountpoint_value_map->pairs[0].value->string);
    if (mountpoint == NULL)
        error(EXIT_FAILURE, errno, "Failed to allocate mountpoint");

    return mountpoint;
}

static struct rbh_fsentry *
get_fsentry_from_metadata_source_with_fid(struct rbh_backend *source,
                                          const struct rbh_value *fid_value)
{
    const struct rbh_filter_projection ALL = {
        .fsentry_mask = RBH_FP_ALL,
        .statx_mask = RBH_STATX_ALL,
    };
    const struct rbh_filter FID_FILTER = {
        .op = RBH_FOP_EQUAL,
        .compare = {
            .field = {
                .fsentry = RBH_FP_ID,
            },
            .value = *fid_value,
        },
    };

    return rbh_backend_filter_one(source, &FID_FILTER, &ALL);
}

static char *
get_mountpoint_from_current_system(struct rbh_backend *source,
                                   struct rbh_backend *target)
{
    struct rbh_value_pair *pwd_pair = NULL;
    const struct rbh_value *fsentry_path;
    struct rbh_value *pwd_value = NULL;
    struct rbh_posix_enrich_ctx ctx;
    struct rbh_fsentry *fsentry;
    struct rbh_value_pair pair;
    ssize_t pwd_pair_count = 1;
    char full_path[PATH_MAX];
    char *mountpoint;
    char *substr;
    int rc;

    pwd_value = malloc(sizeof(*pwd_value));
    if (pwd_value == NULL)
        error(EXIT_FAILURE, errno, "Failed to allocate pwd_value");

    if (getcwd(full_path, sizeof(full_path)) == NULL)
        error(EXIT_FAILURE, errno, "getcwd");

    pwd_value->type = RBH_VT_STRING;
    pwd_value->string = full_path;

    pwd_pair = malloc(sizeof(*pwd_pair));
    if (pwd_pair == NULL)
        error(EXIT_FAILURE, errno, "Failed to allocate pwd_pair");

    pwd_pair->key = "path";
    pwd_pair->value = pwd_value;

    ctx.einfo.inode_xattrs = pwd_pair;
    ctx.einfo.inode_xattrs_count = &pwd_pair_count;

    rc = rbh_backend_get_attribute(target, RBH_LEF_LUSTRE | RBH_LEF_FID,
                                   &ctx, &pair, 1);
    free(pwd_value);
    free(pwd_pair);
    if (rc == -1) {
        fprintf(stderr, "Failed to get FID of current path '%s'\n", full_path);
        return NULL;
    }

    fsentry = get_fsentry_from_metadata_source_with_fid(source, pair.value);
    if (fsentry == NULL) {
        /* XXX: this log may appear a lot, but isn't fatal if there is a
         * mountpoint recorded in the source URI. Should we keep it?
         */
        //fprintf(stderr,
        //        "Failed to find fsentry associated with current path\n");
        return NULL;
    }

    fsentry_path = rbh_fsentry_find_ns_xattr(fsentry, "path");
    if (fsentry_path == NULL) {
        fprintf(stderr, "Cannot get path of '%s' in source URI\n", full_path);
        return NULL;
    }

    substr = strstr(full_path, fsentry_path->string);
    if (substr == NULL) {
        fprintf(stderr,
                "PWD fetched from the database ('%s') is not part of current PWD '%s'\n",
                fsentry_path->string, full_path);
        return NULL;
    }

    *substr = '\0';
    mountpoint = strdup(full_path);
    if (mountpoint == NULL)
        error(EXIT_FAILURE, errno, "Failed to allocate mountpoint");

    return mountpoint;
}

char *
get_mountpoint(struct undelete_context *context)
{
    char *mountpoint;

    mountpoint = get_mountpoint_from_current_system(context->source,
                                                    context->target);

    return mountpoint ? mountpoint :
                        get_mountpoint_from_source(context->source);
}

int
set_targets(const char *target, struct undelete_context *context)
{
    size_t mountpoint_len = strlen(context->mountpoint);

    if (target[0] != '/') {
        char full_path[PATH_MAX];

        if (getcwd(full_path, sizeof(full_path)) == NULL) {
            fprintf(stderr, "Failed to get current path\n");
            return -1;
        }

        if (asprintf(&context->absolute_target_path, "%s/%s",
                     full_path, target) == -1) {
            fprintf(stderr, "Failed create full target path: %s (%d)",
                    strerror(errno), errno);
            return -1;
        }
    } else {
        context->absolute_target_path = strdup(target);
    }

    if (context->absolute_target_path == NULL) {
        fprintf(stderr, "Failed to duplicate target name: %s (%d)",
                strerror(errno), errno);
        return -1;
    }

    if (strncmp(context->absolute_target_path, context->mountpoint,
                mountpoint_len) != 0 ||
        context->absolute_target_path[mountpoint_len] == '\0') {
        fprintf(stderr,
                "Mountpoint recorded '%s' in the source URI isn't in the path to undelete '%s'",
                context->mountpoint, context->absolute_target_path);
        errno = EINVAL;
        return -1;
    }

    context->relative_target_path =
        &context->absolute_target_path[mountpoint_len];

    return 0;
}

