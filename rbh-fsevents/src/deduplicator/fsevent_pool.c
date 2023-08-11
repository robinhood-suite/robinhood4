#include "fsevent_pool.h"
#include "hash.h"
#include "rbh_fsevent_utils.h"

#include <robinhood.h>

#include <lustre/lustre_user.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// FIXME every list element is leaked

struct rbh_fsevent_pool {
    struct rbh_hashmap *pool;
    struct rbh_sstack *list_container;
    struct rbh_sstack *xattr_sequence_container;
    size_t flush_size;
    struct rbh_list_node ids;
    size_t count;
    struct rbh_list_node events;
    struct rbh_list_node free; // TODO implement this
    bool flushed; // I can probably do the flush at the begining of flush?
                  // no need for this boolean
};

struct rbh_fsevent_node {
    struct rbh_fsevent fsevent;
    struct rbh_list_node link;
};

struct rbh_id_node {
    const struct rbh_id *id;
    struct rbh_list_node link;
};

static bool
fsevent_pool_equals(const void *first, const void *second)
{
    return rbh_id_equal(first, second);
}

static size_t
fsevent_pool_hash(const void *key)
{
    return hash_id(key);
}

struct rbh_fsevent_pool *
rbh_fsevent_pool_new(size_t pool_size, size_t flush_size)
{
    struct rbh_fsevent_pool *pool;

    pool = malloc(sizeof(*pool));
    if (!pool)
        return NULL;

    // XXX the stack size should probably be a function of the flush_size
    pool->list_container = rbh_sstack_new(1 << 10);
    if (!pool->list_container) {
        free(pool);
        return NULL;
    }

    pool->xattr_sequence_container = rbh_sstack_new(1 << 10);
    if (!pool->xattr_sequence_container) {
        rbh_sstack_destroy(pool->list_container);
        free(pool);
        return NULL;
    }

    pool->pool = rbh_hashmap_new(fsevent_pool_equals, fsevent_pool_hash,
                                 /* XXX we should add a multiplicator to avoid
                                  * collisions at the expense of memory.
                                  */
                                 pool_size * 100 / 70);
    if (!pool->pool) {
        int save_errno = errno;

        free(pool);
        errno = save_errno;
        return NULL;
    }

    pool->flush_size = flush_size;
    rbh_list_init(&pool->ids);
    pool->count = 0;
    pool->flushed = false;
    rbh_list_init(&pool->events);

    return pool;
}

void
rbh_fsevent_pool_destroy(struct rbh_fsevent_pool *pool)
{
    rbh_hashmap_destroy(pool->pool);
    rbh_sstack_destroy(pool->list_container);
    free(pool);
}

static bool
rbh_fsevent_pool_is_full(struct rbh_fsevent_pool *pool)
{
    return pool->count == pool->flush_size;
}

static int
rbh_fsevent_pool_insert_new_entry(struct rbh_fsevent_pool *pool,
                                  const struct rbh_fsevent *event)
{
    struct rbh_fsevent_node *node;
    struct rbh_id_node *id_node;
    struct rbh_list_node *events;
    int rc;

    events = rbh_sstack_push(pool->list_container, NULL, sizeof(*events));
    if (!events)
        return -1;

    node = rbh_sstack_push(pool->list_container, NULL, sizeof(*node));
    if (!node)
        return -1;

    id_node = rbh_sstack_push(pool->list_container, NULL, sizeof(*id_node));
    if (!id_node)
        return -1;

    rbh_list_init(events);
    rbh_list_add(events, &node->link);
    /* the deep copy is necessary for 2 reasons:
     * 1. the source does not guarantee that the fsevents it generate will be
     * kept alive in the next call to rbh_iter_next on the source
     * 2. we will create new fsevents when merging duplicated ones
     */
    rbh_fsevent_deep_copy(&node->fsevent, event,
                          pool->xattr_sequence_container);

    rc = rbh_hashmap_set(pool->pool, &node->fsevent.id, events);
    if (rc)
        return rc;

    id_node->id = &node->fsevent.id;
    rbh_list_add_tail(&pool->ids, &id_node->link);

    pool->count++;

    return 0;
}

static int
insert_symlink(struct rbh_fsevent_pool *pool,
               struct rbh_fsevent *cached_event)
{
    const struct rbh_value_map *rbh_fsevents_map;
    struct rbh_value symlink_value = {
        .type = RBH_VT_STRING,
        .string = "symlink"
    };
    struct rbh_value_pair symlink_pair = {
        .key = "symlink",
        .value = rbh_sstack_push(pool->xattr_sequence_container,
                                 &symlink_value,
                                 sizeof(symlink_value)
                                 )
    };
    struct rbh_value_pair *tmp;

    rbh_fsevents_map = rbh_fsevent_find_fsevents_map(cached_event);
    assert(rbh_fsevents_map);

    tmp = rbh_sstack_push(pool->xattr_sequence_container,
                          NULL,
                          (rbh_fsevents_map->count + 1) * sizeof(*tmp)
                          );
    memcpy(tmp, rbh_fsevents_map->pairs,
           rbh_fsevents_map->count * sizeof(*tmp)
           );
    memcpy(&tmp[rbh_fsevents_map->count], &symlink_pair, sizeof(symlink_pair));

    void **ptr = (void **)&rbh_fsevents_map->pairs;
    *ptr = tmp;
    size_t *count_ref = (size_t *)&rbh_fsevents_map->count;
    (*count_ref)++;

    return 0;
}

static int
dedup_upsert(struct rbh_fsevent_pool *pool,
             const struct rbh_fsevent *event,
             struct rbh_fsevent *cached_upsert)
{
    const struct rbh_value_map *rbh_fsevents_map;

    rbh_fsevents_map = rbh_fsevent_find_fsevents_map(event);
    /* upsert events either contain "rbh-fsevent": "statx" or
     * "rbh-fsevent": "symlink".
     */
    assert(rbh_fsevents_map);

    for (size_t i = 0; i < rbh_fsevents_map->count; i++) {
        const struct rbh_value_pair *xattr = &rbh_fsevents_map->pairs[i];

        if (!strcmp(xattr->key, "statx")) {
            struct rbh_value *mask;
            mask = (struct rbh_value *)
                cached_upsert->xattrs.pairs[0].value->map.pairs[0].value;

            mask->uint32 |=
                event->xattrs.pairs[0].value->map.pairs[0].value->uint32;

            if (event->upsert.statx) {
                if (cached_upsert->upsert.statx)
                    merge_statx((struct rbh_statx *)cached_upsert->upsert.statx,
                                event->upsert.statx);
                else
                    cached_upsert->upsert.statx = event->upsert.statx;
            }
        } else if (!strcmp(xattr->key, "symlink")) {
            // XXX at most one symlink per inode
            insert_symlink(pool, cached_upsert);
        }
    }


    return 0;
}

static int
deduplicate_event(struct rbh_fsevent_pool *pool, struct rbh_list_node *events,
                  const struct rbh_fsevent *event)
{
    struct rbh_fsevent_node *node;
    bool insert_delete = true;

    if (event->type == RBH_FET_UNLINK) {
        struct rbh_fsevent_node *link = NULL;
        struct rbh_fsevent_node *node;

        rbh_list_foreach(events, node, link)
            if (node->fsevent.type == RBH_FET_LINK &&
                !strcmp(event->link.name, node->fsevent.link.name) &&
                rbh_id_equal(event->link.parent_id,
                             node->fsevent.link.parent_id))
                link = node;

        if (link) {
            rbh_list_del(&link->link);

            if (rbh_list_empty(events)) {
                rbh_hashmap_pop(pool->pool, &event->id);
                // TODO remove id from global list
                pool->count--;
            }
            return 0;
        }
    } else if (event->type == RBH_FET_DELETE) {
        struct rbh_fsevent_node *elem, *tmp;

        rbh_list_foreach_safe(events, elem, tmp, link) {
            if (elem->fsevent.type == RBH_FET_LINK)
                insert_delete = false;

            rbh_list_del(&elem->link);
        }

        if (!insert_delete) {
            if (rbh_list_empty(events)) {
                struct rbh_id_node *elem, *tmp;

                rbh_hashmap_pop(pool->pool, &event->id);
                pool->count--;

                rbh_list_foreach_safe(&pool->ids, elem, tmp, link) {
                    if (rbh_id_equal(&event->id, elem->id)) {
                        // XXX we could keep a reference to this node in the
                        // hash table's element
                        rbh_list_del(&elem->link);
                        break;
                    }
                }
            }
            return 0;
        }
    } else if (event->type == RBH_FET_UPSERT) {
        struct rbh_fsevent_node *upsert = NULL;
        struct rbh_fsevent_node *elem;

        rbh_list_foreach(events, elem, link) {
            if (elem->fsevent.type == RBH_FET_UPSERT) {
                upsert = elem;
                /* with the dedup, only one xattr per event list */
                break;
            }
        }

        if (upsert) {
            dedup_upsert(pool, event, &upsert->fsevent);
            return 0;
        }
    } /* no dedup for links */

    node = rbh_sstack_push(pool->list_container, NULL, sizeof(*node));
    if (!node)
        return -1;

    rbh_fsevent_deep_copy(&node->fsevent, event,
                          pool->xattr_sequence_container);
    if (event->type == RBH_FET_LINK)
        /* move links at the front to insert new entries before any other action
         */
        rbh_list_add(events, &node->link);
    else
        rbh_list_add_tail(events, &node->link);

    return 0;
}

int
rbh_fsevent_pool_push(struct rbh_fsevent_pool *pool,
                      const struct rbh_fsevent *event)
{
    struct rbh_list_node *events;
    int rc;

    if (pool->flushed) {
        struct rbh_fsevent_node *elem, *tmp;

        rbh_list_foreach_safe(&pool->events, elem, tmp, link)
            rbh_list_del(&elem->link);

        pool->flushed = false;
    }

    if (rbh_fsevent_pool_is_full(pool)) {
        errno = ENOSPC;
        return -1;
    }

    events = (void *)rbh_hashmap_get(pool->pool, &event->id);
    if (!events) {

        /* we do not insert NULL values in the map */
        assert(errno == ENOENT);
        rc = rbh_fsevent_pool_insert_new_entry(pool, event);
        if (rc)
            return rc;

        errno = 0;
        return 0;
    }

    rc = deduplicate_event(pool, events, event);
    if (rc)
        return rc;

    return 0;
}

struct rbh_iterator *
rbh_fsevent_pool_flush(struct rbh_fsevent_pool *pool)
{
    size_t count = 0;

    // TODO empty the event list, they could be moved to a free list
    if (pool->count == 0)
        return NULL;

    while (pool->count > 0 && count < pool->flush_size) {
        struct rbh_list_node *first_events;
        struct rbh_id_node *first_id;

        if (rbh_list_empty(&pool->ids))
            break;

        first_id = rbh_list_first(&pool->ids, struct rbh_id_node, link);
        first_events = (void *)rbh_hashmap_get(pool->pool, first_id->id);
        assert(first_events);

        rbh_list_splice_tail(&pool->events, first_events);

        // TODO move this to the free list
        rbh_list_del(&first_id->link);
        rbh_hashmap_pop(pool->pool, first_id->id);
        pool->count--;
        count++;
    }

    pool->flushed = true;

    return rbh_iter_list(&pool->events,
                         offsetof(struct rbh_fsevent_node, link)
                         );
}

