#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "robinhood/backends/posix.h"
#include "robinhood/backends/posix_internal.h"
#include "robinhood/backends/lustre.h"

struct posix_iterator *
lustre_iterator_new(const char *root, const char *entry, int statx_sync_type)
{
    struct posix_iterator *lustre_iter;

    lustre_iter = posix_iterator_new(root, entry, statx_sync_type);
    if (lustre_iter == NULL)
        return NULL;

    lustre_iter->ns_xattrs_callback = NULL;

    return lustre_iter;
}

struct rbh_backend *
rbh_lustre_backend_new(const char *path)
{
    struct posix_backend *lustre;

    lustre = (struct posix_backend *)rbh_posix_backend_new(path);
    if (lustre == NULL)
        return NULL;

    lustre->iter_new = lustre_iterator_new;
    lustre->backend.id = RBH_BI_LUSTRE;
    lustre->backend.name = RBH_LUSTRE_BACKEND_NAME;

    return &lustre->backend;
}
