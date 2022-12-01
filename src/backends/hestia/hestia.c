#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "robinhood/backends/hestia.h"

/*----------------------------------------------------------------------------*
 |                               hestia_backend                               |
 *----------------------------------------------------------------------------*/

struct hestia_backend {
    struct rbh_backend backend;
};

    /*--------------------------------------------------------------------*
     |                             destroy()                              |
     *--------------------------------------------------------------------*/

static void
hestia_backend_destroy(void *backend)
{
    struct hestia_backend *hestia = backend;

    free(hestia);
}

static const struct rbh_backend_operations HESTIA_BACKEND_OPS = {
    .destroy = hestia_backend_destroy,
};

static const struct rbh_backend HESTIA_BACKEND = {
    .id = RBH_BI_HESTIA,
    .name = RBH_HESTIA_BACKEND_NAME,
    .ops = &HESTIA_BACKEND_OPS,
};

struct rbh_backend *
rbh_hestia_backend_new(__attribute__((unused)) const char *path)
{
    struct hestia_backend *hestia;

    hestia = malloc(sizeof(*hestia));
    if (hestia == NULL)
        return NULL;

    hestia->backend = HESTIA_BACKEND;

    return &hestia->backend;
}
