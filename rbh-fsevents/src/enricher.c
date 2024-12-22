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
enrich_iter_builder_from_backend(const char *type,
                                 struct rbh_backend *backend,
                                 const char *mount_path)
{
    if (!strcmp(type, "posix"))
        return posix_enrich_iter_builder(backend, mount_path);
#ifdef HAVE_LUSTRE
    else if (!strcmp(type, "lustre"))
        return lustre_enrich_iter_builder(backend, mount_path);
#endif
#ifdef HAVE_HESTIA
    else if (!strcmp(type, "hestia"))
        return hestia_enrich_iter_builder(backend);
#endif

    error(EX_USAGE, EINVAL, "enricher type '%s' not allowed", type);
    __builtin_unreachable();
}
