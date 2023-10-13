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
            return posix_enrich_iter_builder(backend, mount_path);
#ifdef HAVE_LUSTRE
        case RBH_BI_LUSTRE:
            return lustre_enrich_iter_builder(backend, mount_path);
#endif
#ifdef HAVE_HESTIA
        case RBH_BI_HESTIA:
            return hestia_enrich_iter_builder(backend);
#endif
        default:
            error(EX_USAGE, EINVAL, "enricher type not allowed");
    }

    __builtin_unreachable();
}
