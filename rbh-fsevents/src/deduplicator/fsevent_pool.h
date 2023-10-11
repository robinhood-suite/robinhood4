#ifndef FSEVENT_POOL_H
#define FSEVENT_POOL_H

#include <robinhood/hashmap.h>
#include <robinhood/fsevent.h>

struct rbh_fsevent_pool;

struct rbh_fsevent_pool *
rbh_fsevent_pool_new(size_t batch_size, size_t flush_size);

void
rbh_fsevent_pool_destroy(struct rbh_fsevent_pool *pool);

int
rbh_fsevent_pool_push(struct rbh_fsevent_pool *pool,
                      const struct rbh_fsevent *event);

struct rbh_iterator *
rbh_fsevent_pool_flush(struct rbh_fsevent_pool *pool);

#endif
