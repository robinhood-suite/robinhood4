#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <jansson.h>

#include "robinhood/backends/hestia.h"
#include "robinhood/sstack.h"
#include "robinhood/statx.h"

#include "enricher.h"
#include "internals.h"

/*----------------------------------------------------------------------------*
 *                           hestia_backend_enrich                            *
 *----------------------------------------------------------------------------*/

static struct rbh_iterator *
hestia_enrich_iter_builder_build_iter(void *_builder,
                                      struct rbh_iterator *fsevents)
{
    struct enrich_iter_builder *builder = _builder;

    (void) builder;
    (void) fsevents;

    return NULL;
}

void
hestia_enrich_iter_builder_destroy(void *_builder)
{
    struct enrich_iter_builder *builder = _builder;

    free(builder->backend);
    free(builder);
}

static const struct enrich_iter_builder_operations
HESTIA_ENRICH_ITER_BUILDER_OPS = {
    .build_iter = hestia_enrich_iter_builder_build_iter,
    .destroy = hestia_enrich_iter_builder_destroy,
};

const struct enrich_iter_builder HESTIA_ENRICH_ITER_BUILDER = {
    .name = "hestia",
    .ops = &HESTIA_ENRICH_ITER_BUILDER_OPS,
};
