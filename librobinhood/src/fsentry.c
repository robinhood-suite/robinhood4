/* This file is part of the RobinHood Library
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

#include "robinhood/fsentry.h"
#include "robinhood/statx.h"

#include "utils.h"
#include "value.h"

struct rbh_fsentry *
rbh_fsentry_new(const struct rbh_id *id, const struct rbh_id *parent_id,
                const char *name, const struct rbh_statx *statxbuf,
                const struct rbh_value_map *ns_xattrs,
                const struct rbh_value_map *xattrs, const char *symlink)
{
    struct rbh_fsentry *fsentry;
    size_t symlink_length = 0;
    size_t name_length = 0;
    size_t size = 0;
    char *data;

    if (symlink) {
        if (statxbuf && (statxbuf->stx_mask & RBH_STATX_TYPE)
                && !S_ISLNK(statxbuf->stx_mode)) {
            errno = EINVAL;
            return NULL;
        }
        symlink_length = strlen(symlink) + 1;
        size += symlink_length;
    }
    if (id)
        size += id->size;
    if (parent_id)
        size += parent_id->size;
    if (name) {
        name_length = strlen(name) + 1;
        size += name_length;
    }
    if (statxbuf) {
        size = sizealign(size, alignof(*fsentry->statx));
        size += sizeof(*statxbuf);
    }
    if (ns_xattrs) {
        size = sizealign(size, alignof(*fsentry->xattrs.ns.pairs));
        if (value_map_data_size(ns_xattrs) < 0)
            return NULL;
        size += value_map_data_size(ns_xattrs);
    }
    if (xattrs) {
        size = sizealign(size, alignof(*fsentry->xattrs.inode.pairs));
        if (value_map_data_size(xattrs) < 0)
            return NULL;
        size += value_map_data_size(xattrs);
    }

    fsentry = malloc(sizeof(*fsentry) + size);
    if (fsentry == NULL)
        return NULL;
    data = fsentry->symlink;

    /* fsentry->mask */
    fsentry->mask = 0;

    /* fsentry->symlink (set first because it is a flexible array) */
    if (symlink) {
        assert(size >= symlink_length);
        data = mempcpy(data, symlink, symlink_length);
        size -= symlink_length;
        fsentry->mask |= RBH_FP_SYMLINK;
    }

    /* fsentry->id */
    if (id) {
        int rc = rbh_id_copy(&fsentry->id, id, &data, &size);
        assert(rc == 0);
        fsentry->mask |= RBH_FP_ID;
    }

    /* fsentry->parent_id */
    if (parent_id) {
        int rc = rbh_id_copy(&fsentry->parent_id, parent_id, &data, &size);
        assert(rc == 0);
        fsentry->mask |= RBH_FP_PARENT_ID;
    }

    /* fsentry->name */
    if (name) {
        assert(size >= name_length);
        fsentry->name = data;
        data = mempcpy(data, name, name_length);
        size -= name_length;
        fsentry->mask |= RBH_FP_NAME;
    }

    /* fsentry->statx */
    if (statxbuf) {
        struct rbh_statx *tmp;

        tmp = aligned_memalloc(alignof(*tmp), sizeof(*tmp), &data, &size);
        assert(tmp);
        *tmp = *statxbuf;
        fsentry->statx = tmp;
        fsentry->mask |= RBH_FP_STATX;
    }

    /* fsentry->xattrs.ns */
    if (ns_xattrs) {
        int rc = value_map_copy(&fsentry->xattrs.ns, ns_xattrs, &data, &size);
        /* If `ns_xattrs' contained invalid data, value_map_data_size() would
         * have caught it.
         */
        assert(rc == 0);
        fsentry->mask |= RBH_FP_NAMESPACE_XATTRS;
    }

    /* fsentry->xattrs.inode */
    if (xattrs) {
        int rc = value_map_copy(&fsentry->xattrs.inode, xattrs, &data, &size);
        /* If `xattrs' contained invalid data, value_map_data_size() would have
         * caught it.
         */
        assert(rc == 0);
        fsentry->mask |= RBH_FP_INODE_XATTRS;
    }

    /* scan-build: intentional dead store */
    (void)data;
    (void)size;

    return fsentry;
}

static const struct rbh_value *
rbh_map_find(const struct rbh_value_map *map, const char *key)
{
    for (size_t i = 0; i < map->count; i++)
        if (!strcmp(map->pairs[i].key, key))
            return map->pairs[i].value;

    return NULL;
}

const struct rbh_value *
rbh_fsentry_find_inode_xattr(const struct rbh_fsentry *entry,
                             const char *key_to_find)
{
    const struct rbh_value_map *map = &entry->xattrs.inode;
    const struct rbh_value *value = NULL;
    char *key = strdup(key_to_find);
    char *subkey;
    char *next;

    if (key == NULL)
        return NULL;

    subkey = strtok(key, ".");

    while (subkey != NULL) {
        next = strtok(NULL, ".");

        if (next != NULL) {
            value = rbh_map_find(map, subkey);
            if (value == NULL || value->type != RBH_VT_MAP) {
                free(key);
                return NULL;
            }

            map = &value->map;
        } else {
            value = rbh_map_find(map, subkey);
            break;
        }

        subkey = next;
    }

    free(key);

    return value;
}

const char *
fsentry_path(const struct rbh_fsentry *fsentry)
{
    if (!(fsentry->mask & RBH_FP_NAMESPACE_XATTRS))
        return NULL;

    for (size_t i = 0; i < fsentry->xattrs.ns.count; i++) {
        const struct rbh_value_pair *pair = &fsentry->xattrs.ns.pairs[i];

        if (strcmp(pair->key, "path"))
            continue;

        if (pair->value->type != RBH_VT_STRING)
            /* XXX: should probably say something... */
            continue;

        return pair->value->string;
    }

    return NULL;
}

const char *
fsentry_relative_path(const struct rbh_fsentry *fsentry)
{
    const char *path = fsentry_path(fsentry);

    if (path[0] == '/' && path[1] == '\0')
        return ".";
    else
        return &path[1];
}
