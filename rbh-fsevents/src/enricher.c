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
#include "enrichers/posix/internals.h"

struct enrich_iter_builder *
enrich_iter_builder_from_backend(const char *type,
                                 struct rbh_backend *backend,
                                 const char *mount_path)
{
    switch (backend->id) {
    case RBH_BI_POSIX:
        return posix_enrich_iter_builder(backend, type, mount_path);
#ifdef HAVE_HESTIA
    case RBH_BI_HESTIA:
        return hestia_enrich_iter_builder(backend);
#endif
    default:
        break;
    }

    error(EX_USAGE, EINVAL, "enricher type '%s' not allowed", type);
    __builtin_unreachable();
}
