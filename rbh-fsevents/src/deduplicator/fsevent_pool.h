#ifndef FSEVENT_POOL_H
#define FSEVENT_POOL_H

#include <robinhood/hashmap.h>
#include <robinhood/fsevent.h>

struct rbh_fsevent_pool;

struct rbh_fsevent_pool *
rbh_fsevent_pool_new(size_t pool_size, size_t flush_size);

void
rbh_fsevent_pool_destroy(struct rbh_fsevent_pool *pool);

enum fsevent_pool_push_res {
    POOL_INSERT_OK,
    POOL_FULL,
    POOL_INSERT_FAILED,
    POOL_ALREADY_FULL,
};

enum fsevent_pool_push_res
rbh_fsevent_pool_push(struct rbh_fsevent_pool *pool,
                      const struct rbh_fsevent *event);

struct rbh_iterator *
rbh_fsevent_pool_flush(struct rbh_fsevent_pool *pool);

#endif
