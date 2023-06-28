/* SPDX-License-Identifier: LGPL-3.0-or-later */

#include <error.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "enricher.h"
#include "enrichers/internals.h"

struct enrich_iter_builder *
enrich_iter_builder_from_backend(struct rbh_backend *backend,
                                 const char *mount_path)
{
    struct enrich_iter_builder *builder;

    builder = malloc(sizeof(*builder));
    if (builder == NULL)
        error(EXIT_FAILURE, errno, "malloc");

    switch (backend->id) {
        case RBH_BI_POSIX:
            *builder = POSIX_ENRICH_ITER_BUILDER;
            break;
#ifdef HAVE_LUSTRE
        case RBH_BI_LUSTRE:
            *builder = LUSTRE_ENRICH_ITER_BUILDER;
            break;
#endif
        default:
            error(EX_USAGE, EINVAL, "enricher type not allowed");
    }

    builder->backend = backend;
    builder->mount_fd = open(mount_path, O_RDONLY | O_CLOEXEC);
    if (builder->mount_fd == -1)
        error(EXIT_FAILURE, errno, "open: %s", mount_path);

    builder->mount_path = strdup(mount_path);
    if (builder->mount_path == NULL)
        error(EXIT_FAILURE, ENOMEM, "strdup: %s", mount_path);

    return builder;
}
