/* This file is part of RobinHood 4
 * Copyright (C) 2025 Commissariat a l'energie atomique et aux energies
 *                    alternatives
 *
 * SPDX-License-Identifer: LGPL-3.0-or-later
 */

#ifndef ROBINHOOD_POSIX_EXTENSION_H
#define ROBINHOOD_POSIX_EXTENSION_H

/**
 * @file
 *
 * External header which defines the structures and functions usable by posix
 * extensions. Access to these function require linking to the posix backend's
 * library.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <robinhood/plugin.h>
#include <robinhood/plugins/backend.h>

#include <robinhood/backends/common.h>
#include <robinhood/iterator.h>
#include <robinhood/sstack.h>

struct rbh_value_pair;
struct rbh_statx;
struct entry_info;
struct rbh_sstack;

typedef int (*enricher_t)(struct entry_info *einfo,
                          uint64_t flags,
                          struct rbh_value_pair *pairs,
                          size_t pairs_count,
                          struct rbh_sstack *values);

typedef struct rbh_mut_iterator *(*iter_new_t)(const char *, const char *, int);

struct posix_iterator {
    struct rbh_mut_iterator iterator;
    const struct rbh_posix_extension **enrichers;
    int statx_sync_type;
    size_t prefix_len;
    bool skip_error;
    char *path;
};

/**
 * Structure containing the result of fsentry_from_any
 */
struct fsentry_id_pair {
    /**
     * rbh_id of the rbh_fsentry,
     * used by the POSIX backend, corresponds to the fsentry's parent
     */
    struct rbh_id *id;
    struct rbh_fsentry *fsentry;
};

struct rbh_posix_extension {
    struct rbh_plugin_extension extension;
    iter_new_t iter_new;
    enricher_t enrich;
    int (*setup_enricher)(void);
};

struct rbh_posix_enrich_ctx {
    struct entry_info einfo;
    struct rbh_sstack *values;
};

int
rbh_posix_enrich_open_by_id(struct rbh_posix_enrich_ctx *ctx,
                            int parent_fd,
                            const struct rbh_id *id);

int
rbh_posix_enrich_statx(struct rbh_posix_enrich_ctx *ctx, int flags,
                       unsigned int mask, struct rbh_statx *restrict statxbuf);

struct posix_backend {
    struct rbh_backend backend;
    struct rbh_mut_iterator *(*iter_new)(const char *, const char *, int);
    char *root;
    int statx_sync_type;
    const struct rbh_posix_extension **enrichers;
};

struct posix_branch_backend {
    struct posix_backend posix;
    struct rbh_id id;
    char *path;
};

static inline const struct rbh_posix_extension *
rbh_posix_load_extension(const struct rbh_plugin *plugin, const char *name)
{
    return (const struct rbh_posix_extension *)
        rbh_plugin_load_extension(plugin, name);
}

int
posix_iterator_setup(struct posix_iterator *iter,
                     const char *root,
                     const char *entry,
                     int statx_sync_type);

struct rbh_value_map
rbh_posix_get_source_map(bool is_plugin, const char *extension_name,
                         struct rbh_sstack *sstack);

int
rbh_posix_enrichers_list(struct rbh_config *config, const char *type,
                         struct rbh_value *enrichers);

struct rbh_id *
id_from_fd(int fd, short backend_id);

char *
freadlink(int fd, const char *path, size_t *size_);

void
build_pair_nb_children(struct rbh_value_pair *pair, int nb_children,
                       struct rbh_sstack *sstack);

struct rbh_fsentry *
build_fsentry_nb_children(struct rbh_id *id, int nb_children,
                          struct rbh_sstack *sstack);

bool
fsentry_from_any(struct fsentry_id_pair *fip, const struct rbh_value *path,
                 char *accpath, struct rbh_id *entry_id,
                 struct rbh_id *parent_id, char *name, int statx_sync_type,
                 const struct rbh_posix_extension **enrichers);

char *
id2path(const char *root, const struct rbh_id *id);

#endif
