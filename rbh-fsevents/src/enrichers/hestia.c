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

static const void *
hestia_enricher_iter_next(void *iterator)
{
    struct enricher *enricher = iterator;
    const void *fsevent;

    (void) enricher;
    (void) fsevent;

    return NULL;
}

void
hestia_enricher_iter_destroy(void *iterator)
{
    struct enricher *enricher = iterator;

    rbh_iter_destroy(enricher->fsevents);
    free(enricher->pairs);
    free(enricher);
}

static const struct rbh_iterator_operations HESTIA_ENRICHER_ITER_OPS = {
    .next = hestia_enricher_iter_next,
    .destroy = hestia_enricher_iter_destroy,
};

static const struct rbh_iterator HESTIA_ENRICHER_ITERATOR = {
    .ops = &HESTIA_ENRICHER_ITER_OPS,
};

#define INITIAL_PAIR_COUNT (1 << 7)

struct rbh_iterator *
hestia_iter_enrich(struct rbh_iterator *fsevents)
{
    struct rbh_value_pair *pairs;
    struct enricher *enricher;

    pairs = reallocarray(NULL, INITIAL_PAIR_COUNT, sizeof(*pairs));
    if (pairs == NULL)
        return NULL;

    enricher = malloc(sizeof(*enricher));
    if (enricher == NULL) {
        int save_errno = errno;

        free(pairs);
        errno = save_errno;
        return NULL;
    }

    enricher->iterator = HESTIA_ENRICHER_ITERATOR;
    enricher->backend = NULL;
    enricher->fsevents = fsevents;
    enricher->mount_fd = -1;
    enricher->mount_path = NULL;
    enricher->pairs = pairs;
    enricher->pair_count = INITIAL_PAIR_COUNT;
    enricher->symlink = NULL;

    return &enricher->iterator;
}

/*----------------------------------------------------------------------------*
 *                           hestia_backend_enrich                            *
 *----------------------------------------------------------------------------*/

static struct rbh_iterator *
hestia_build_enrich_iter(void *_builder, struct rbh_iterator *fsevents)
{
    (void) _builder;

    return hestia_iter_enrich(fsevents);
}

void
hestia_iter_builder_destroy(void *_builder)
{
    struct enrich_iter_builder *builder = _builder;

    free(builder->backend);
    free(builder);
}

static const struct enrich_iter_builder_operations
HESTIA_ENRICH_ITER_BUILDER_OPS = {
    .build_iter = hestia_build_enrich_iter,
    .destroy = hestia_iter_builder_destroy,
};

const struct enrich_iter_builder HESTIA_ENRICH_ITER_BUILDER = {
    .name = "hestia",
    .ops = &HESTIA_ENRICH_ITER_BUILDER_OPS,
};
