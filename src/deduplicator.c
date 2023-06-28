/* SPDX-License-Identifer: LGPL-3.0-or-later */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <stdlib.h>

#include <robinhood/itertools.h>
#include <robinhood/ring.h>

#include "deduplicator.h"

struct deduplicator {
    struct rbh_mut_iterator batches;

    struct source *source;
};

/*----------------------------------------------------------------------------*
 |                                deduplicator                                |
 *----------------------------------------------------------------------------*/

static void *
deduplicator_iter_next(void *iterator)
{
    struct deduplicator *deduplicator = iterator;
    const struct rbh_fsevent *fsevent;

    fsevent = rbh_iter_next(&deduplicator->source->fsevents);
    if (fsevent == NULL)
        return NULL;

    return rbh_iter_array(fsevent, sizeof(fsevent), 1);
}

static void
deduplicator_iter_destroy(void *iterator)
{
    struct deduplicator *deduplicator = iterator;

    free(deduplicator);
}

static const struct rbh_mut_iterator_operations DEDUPLICATOR_ITER_OPS = {
    .next = deduplicator_iter_next,
    .destroy = deduplicator_iter_destroy,
};

static const struct rbh_mut_iterator DEDUPLICATOR_ITERATOR = {
    .ops = &DEDUPLICATOR_ITER_OPS,
};

struct rbh_mut_iterator *
deduplicator_new(size_t count __attribute__((unused)), struct source *source)
{
    struct deduplicator *deduplicator;

    deduplicator = malloc(sizeof(*deduplicator));
    if (deduplicator == NULL)
        return NULL;

    deduplicator->batches = DEDUPLICATOR_ITERATOR;
    deduplicator->source = source;
    return &deduplicator->batches;
}
