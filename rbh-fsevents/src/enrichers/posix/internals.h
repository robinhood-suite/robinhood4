/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifndef ENRICHER_INTERNALS_H
#define ENRICHER_INTERNALS_H

#include <robinhood.h>

struct enricher;

typedef int (*enrich_xattr_t)(struct enricher *enricher,
                              const struct rbh_value_pair *xattr
                              );

struct posix_enricher {
    enrich_xattr_t enrich_xattr;
};

struct enricher {
    struct rbh_iterator iterator;
    struct rbh_backend *backend;

    struct rbh_iterator *fsevents;
    int mount_fd;
    const char *mount_path;

    struct rbh_value_pair *pairs;
    size_t pair_count;

    struct rbh_fsevent fsevent;
    struct rbh_statx statx;
    char *symlink;

    bool skip_error;
    struct posix_enricher *extension_enrichers;
    size_t n_extensions;
};

/*----------------------------------------------------------------------------*
 *                              posix internals                               *
 *----------------------------------------------------------------------------*/

/* The Linux VFS doesn't allow for symlinks of more than 64KiB */
#define SYMLINK_MAX_SIZE (1 << 16)

int posix_enrich(struct enricher *enricher,
                 const struct rbh_value_pair *partial,
                 struct rbh_value_pair **pairs,
                 size_t *pair_count,
                 struct rbh_fsevent *enriched,
                 const struct rbh_fsevent *original);

struct rbh_iterator *
posix_iter_enrich(struct rbh_iterator *fsevents, int mount_fd,
                  const char *mount_path, bool skip_error);

void
posix_enricher_iter_destroy(void *iterator);

void
posix_enrich_iter_builder_destroy(void *_enrich);

/*----------------------------------------------------------------------------*
 *                       enrich iter builder interfaces                       *
 *----------------------------------------------------------------------------*/

struct enrich_iter_builder *
posix_enrich_iter_builder(struct rbh_backend *backend, const char *mount_path);

#ifdef HAVE_LUSTRE
struct enrich_iter_builder *
lustre_enrich_iter_builder(struct rbh_backend *backend, const char *mount_path);
#endif

#ifdef HAVE_HESTIA
struct enrich_iter_builder *
hestia_enrich_iter_builder(struct rbh_backend *backend);
#endif

#endif
