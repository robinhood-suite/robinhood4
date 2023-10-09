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
    size_t size; /* maximum number of ids allowed in the pool */
    struct rbh_hashmap *pool; /* container of lists of events per id */
    struct rbh_sstack *list_container; /* container of list elements */
    struct rbh_sstack *xattr_sequence_container; /* container for fsevents */
    size_t flush_size; /* number of elements to be flushed when the pool is full
                        */
    struct rbh_list_node ids; /* list of rbh_id that were inserted in the pool
                               * ordered by time of insertion
                               */
    size_t count; /* total number of ids in the pool */
    struct rbh_list_node events; /* list of flushed events that the user of the
                                  * pool will iterate over
                                  */
    // TODO implement this
    struct rbh_list_node free;
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

    if (flush_size > pool_size || flush_size == 0)
        return NULL;

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
        rbh_sstack_destroy(pool->xattr_sequence_container);
        rbh_sstack_destroy(pool->list_container);
        errno = save_errno;
        return NULL;
    }

    pool->flush_size = flush_size;
    pool->size = pool_size;
    rbh_list_init(&pool->ids);
    pool->count = 0;
    rbh_list_init(&pool->events);
    rbh_list_init(&pool->free);

    return pool;
}

void
rbh_fsevent_pool_destroy(struct rbh_fsevent_pool *pool)
{
    rbh_hashmap_destroy(pool->pool);
    rbh_sstack_destroy(pool->list_container);
    rbh_sstack_destroy(pool->xattr_sequence_container);
    free(pool);
}

static bool
rbh_fsevent_pool_is_full(struct rbh_fsevent_pool *pool)
{
    return pool->count == pool->size;
}

static int
rbh_fsevent_pool_insert_new_entry(struct rbh_fsevent_pool *pool,
                                  const struct rbh_fsevent *event)
{
    struct rbh_fsevent_node *node;
    struct rbh_list_node *events;
    struct rbh_id_node *id_node;
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
     * 1. the source does not guarantee that the fsevents it generates will be
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
insert_partial_xattr(struct rbh_fsevent_pool * pool,
                     struct rbh_fsevent *cached_event,
                     const struct rbh_value *partial_xattr)
{
    const struct rbh_value_map *rbh_fsevents_map;
    const struct rbh_value *xattrs_sequence;
    size_t *count_ref;

    /* we have at least one xattr in the cached event */
    assert(cached_event->xattrs.count >= 1);

    rbh_fsevents_map = rbh_fsevent_find_fsevents_map(cached_event);
    if (!rbh_fsevents_map) {
        /* "rbh-fsevents" may not exist if first xattr was complete */
        struct rbh_value rbh_fsevent_value = {
            .type = RBH_VT_MAP,
            .map = {
                .count = 0,
                .pairs = NULL,
            },
        };
        struct rbh_value_pair rbh_fsevent_pair = {
            .key = "rbh-fsevents",
            .value = rbh_sstack_push(pool->xattr_sequence_container,
                                     &rbh_fsevent_value,
                                     sizeof(rbh_fsevent_value)
                                     ),
        };
        struct rbh_value_pair *tmp;

        tmp = rbh_sstack_push(pool->xattr_sequence_container,
                              NULL,
                              sizeof(*tmp) * (cached_event->xattrs.count + 1)
                              );
        if (!tmp)
            return -1;

        memcpy(tmp, cached_event->xattrs.pairs,
               sizeof(*tmp) * cached_event->xattrs.count
               );
        memcpy(&tmp[cached_event->xattrs.count], &rbh_fsevent_pair,
               sizeof(rbh_fsevent_pair)
               );
        rbh_fsevents_map = &tmp[cached_event->xattrs.count].value->map;
        cached_event->xattrs.count++;
        cached_event->xattrs.pairs = tmp;
    }

    xattrs_sequence = rbh_map_find(rbh_fsevents_map, "xattrs");
    if (!xattrs_sequence) {
        /* the map may not exist if first event was "lustre" */
        struct rbh_value xattr_string = {
            .type = RBH_VT_STRING,
            .string = rbh_sstack_push(pool->xattr_sequence_container,
                                      partial_xattr->string,
                                      strlen(partial_xattr->string) + 1
                                      ),
        };
        struct rbh_value xattrs_sequence = {
            .type = RBH_VT_SEQUENCE,
            .sequence = {
                .count = 1,
                .values = rbh_sstack_push(pool->xattr_sequence_container,
                                          &xattr_string, sizeof(xattr_string)
                                          ),
                // TODO check non NULL
            },
        };
        struct rbh_value_pair xattrs_pair = {
            .key = "xattrs",
            .value = rbh_sstack_push(pool->xattr_sequence_container,
                                     &xattrs_sequence, sizeof(xattrs_sequence)
                                     ),
            // TODO check non null
        };
        struct rbh_value_pair *tmp;

        tmp = rbh_sstack_push(pool->xattr_sequence_container,
                              NULL,
                              (rbh_fsevents_map->count + 1) *
                              sizeof(*rbh_fsevents_map->pairs)
                              );
        memcpy(tmp, rbh_fsevents_map->pairs,
               rbh_fsevents_map->count * sizeof(*rbh_fsevents_map->pairs)
               );
        memcpy(&tmp[rbh_fsevents_map->count],
               &xattrs_pair, sizeof(xattrs_pair)
               );

        void **ptr = (void **)&rbh_fsevents_map->pairs;
        *ptr = tmp;

        count_ref = (size_t *)&rbh_fsevents_map->count;
        (*count_ref)++;

        return 0;
    }
    struct rbh_value *tmp;

    assert(xattrs_sequence->type == RBH_VT_SEQUENCE);

    tmp = rbh_sstack_push(pool->xattr_sequence_container,
                          NULL,
                          (xattrs_sequence->sequence.count + 1) *
                          sizeof(*xattrs_sequence->sequence.values)
                          );
    if (!tmp)
        return -1;

    void **ptr = (void **)&xattrs_sequence->sequence.values;
    count_ref = (size_t *)&xattrs_sequence->sequence.count;

    struct rbh_value xattr_string = {
        .type = RBH_VT_STRING,
        .string = rbh_sstack_push(pool->xattr_sequence_container,
                                  partial_xattr->string,
                                  strlen(partial_xattr->string) + 1
                                  ),
    };

    memcpy(tmp, xattrs_sequence->sequence.values,
           xattrs_sequence->sequence.count *
           sizeof(*xattrs_sequence->sequence.values)
           );
    memcpy(&tmp[xattrs_sequence->sequence.count], &xattr_string,
           sizeof(xattr_string)
           );

    *ptr = tmp;
    (*count_ref)++;

    return 0;
}

static void
dedup_partial_xattr(struct rbh_fsevent_pool *pool,
                    struct rbh_fsevent *cached_event,
                    const struct rbh_value *partial_xattr)
{
    const struct rbh_value *cached_partial_xattr;

    assert(partial_xattr->type == RBH_VT_STRING);
    cached_partial_xattr = rbh_fsevent_find_partial_xattr(
        // FIXME this does not work
        cached_event, partial_xattr->string
        );

    if (cached_partial_xattr)
        /* xattr found, do not add it to the cached fsevent */
        return;

    insert_partial_xattr(pool, cached_event, partial_xattr);
}

static int
insert_enrich_element(struct rbh_fsevent_pool *pool,
                      struct rbh_fsevent *cached_event,
                      const struct rbh_value_pair *xattr)
{
    const struct rbh_value_map *rbh_fsevents_map;

    rbh_fsevents_map = rbh_fsevent_find_fsevents_map(cached_event);

    if (!rbh_fsevents_map) {
        /* "rbh-fsevents" may not exist if first xattr was complete */
        struct rbh_value rbh_fsevents_value = {
            .type = RBH_VT_MAP,
            .map = {
                .count = 1,
                .pairs = rbh_sstack_push(pool->xattr_sequence_container,
                                         xattr,
                                         sizeof(*xattr)),
            },
        };
        struct rbh_value_pair rbh_fsevents_map = {
            .key = "rbh-fsevents",
            .value = rbh_sstack_push(pool->xattr_sequence_container,
                                     &rbh_fsevents_value,
                                     sizeof(rbh_fsevents_value)),
        };
        struct rbh_value_pair *tmp;

        tmp = rbh_sstack_push(pool->xattr_sequence_container,
                              NULL,
                              sizeof(*tmp) * (cached_event->xattrs.count + 1)
                              );
        if (!tmp)
            return -1;

        memcpy(tmp, cached_event->xattrs.pairs,
               sizeof(*cached_event->xattrs.pairs) * cached_event->xattrs.count
               );
        memcpy(&tmp[cached_event->xattrs.count], &rbh_fsevents_map,
               sizeof(rbh_fsevents_map));

        cached_event->xattrs.pairs = tmp;
        cached_event->xattrs.count++;

        return 0;
    }

    size_t *count_ref = (size_t *)&rbh_fsevents_map->count;
    void **ptr = (void **)&rbh_fsevents_map->pairs;

    struct rbh_value_pair *tmp;

    tmp = rbh_sstack_push(pool->xattr_sequence_container,
                          NULL,
                          sizeof(*tmp) * (rbh_fsevents_map->count + 1));
    if (!tmp)
        // TODO error(...);
        return -1;

    memcpy(tmp, rbh_fsevents_map->pairs,
           rbh_fsevents_map->count * sizeof(*rbh_fsevents_map->pairs));
    memcpy(&tmp[rbh_fsevents_map->count], xattr, sizeof(*xattr));

    *ptr = tmp;
    (*count_ref)++;

    return 0;
}

static void
dedup_enrich_element(struct rbh_fsevent_pool *pool,
                     struct rbh_fsevent *cached_event,
                     const struct rbh_value_pair *xattr)
{
    const struct rbh_value_pair *cached_xattr;

    cached_xattr = rbh_fsevent_find_enrich_element(cached_event, xattr->key);
    if (cached_xattr)
        /* xattr found, do not add it to the cached fsevent */
        // FIXME if the value is different, update it
        return;

    insert_enrich_element(pool, cached_event, xattr);
}

static int
insert_xattr(struct rbh_fsevent_pool *pool, struct rbh_fsevent *cached_event,
             const struct rbh_value_pair *xattr)
{
    struct rbh_value_pair *tmp;

    tmp = rbh_sstack_push(pool->xattr_sequence_container,
                          NULL,
                          sizeof(*tmp) * (cached_event->xattrs.count + 1));
    if (!tmp)
        return -1;

    memcpy(tmp, cached_event->xattrs.pairs,
           cached_event->xattrs.count * sizeof(*tmp));
    memcpy(&tmp[cached_event->xattrs.count], xattr, sizeof(*xattr));

    cached_event->xattrs.count++;
    cached_event->xattrs.pairs = tmp;

    return 0;
}

static void
dedup_xattr(struct rbh_fsevent_pool *pool,
            struct rbh_fsevent *cached_event,
            const struct rbh_value_pair *xattr)
{
    const struct rbh_value_pair *cached_xattr;

    cached_xattr = rbh_fsevent_find_xattr(cached_event, xattr->key);
    if (cached_xattr)
        /* xattr found, do not add it to the cached fsevent */
        // FIXME if the value is different, update it
        return;

    insert_xattr(pool, cached_event, xattr);
}

static void
dedup_xattrs(struct rbh_fsevent_pool *pool,
             const struct rbh_fsevent *event,
             struct rbh_fsevent *cached_xattr)
{
    for (size_t i = 0; i < event->xattrs.count; i++) {
        const struct rbh_value_pair *xattr = &event->xattrs.pairs[i];

        if (!strcmp(xattr->key, "rbh-fsevents")) {
            const struct rbh_value_map *sub_map;

            assert(xattr->value->type == RBH_VT_MAP);
            sub_map = (void *)&xattr->value->map;

            for (size_t i = 0; i < sub_map->count; i++) {
                if (!strcmp(sub_map->pairs[i].key, "xattrs")) {
                    const struct rbh_value *to_enrich;

                    to_enrich = (void *)sub_map->pairs[i].value;

                    assert(to_enrich->type == RBH_VT_SEQUENCE);

                    for (size_t i = 0; i < to_enrich->sequence.count;
                         i++) {
                        const struct rbh_value *partial_xattr;

                        partial_xattr = (void *)&to_enrich->sequence.values[i];
                        dedup_partial_xattr(pool, cached_xattr, partial_xattr);
                    }
                } else {
                    dedup_enrich_element(pool, cached_xattr,
                                         &sub_map->pairs[i]);
                }
            }
        } else {
            dedup_xattr(pool, cached_xattr, xattr);
        }
    }
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
                                 sizeof(symlink_value))
    };
    struct rbh_value_pair *tmp;

    rbh_fsevents_map = rbh_fsevent_find_fsevents_map(cached_event);
    assert(rbh_fsevents_map);

    tmp = rbh_sstack_push(pool->xattr_sequence_container,
                          NULL,
                          (rbh_fsevents_map->count + 1) * sizeof(*tmp));
    memcpy(tmp, rbh_fsevents_map->pairs,
           rbh_fsevents_map->count * sizeof(*tmp));
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
    /* upsert events either contain "rbh-fsevents": "statx" or
     * "rbh-fsevents": "symlink".
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

        rbh_list_foreach(events, node, link) {
            if (node->fsevent.type == RBH_FET_LINK &&
                !strcmp(event->link.name, node->fsevent.link.name) &&
                rbh_id_equal(event->link.parent_id,
                             node->fsevent.link.parent_id))
                link = node;
        }

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

            return 0;
        }
    } else if (event->type == RBH_FET_XATTR) {
        struct rbh_fsevent_node *xattr_event = NULL;
        struct rbh_fsevent_node *elem;

        // XXX instead of a list, we could have one xattr, a list of links, a
        // list of unlinks, one delete...
        rbh_list_foreach(events, elem, link) {
            if (elem->fsevent.type == RBH_FET_XATTR) {
                xattr_event = elem;
                /* with the dedup, only one xattr per event list */
                break;
            }
        }

        if (xattr_event) {
            dedup_xattrs(pool, event, &xattr_event->fsevent);

            return 0;
        }
    } else if (event->type == RBH_FET_UPSERT) {
        struct rbh_fsevent_node *upsert = NULL;
        struct rbh_fsevent_node *elem;

        rbh_list_foreach(events, elem, link) {
            if (elem->fsevent.type == RBH_FET_UPSERT) {
                upsert = elem;
                /* with the dedup, only one upsert per event list */
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

    if (rbh_fsevent_pool_is_full(pool)) {
        errno = ENOSPC;
        return 0;
    }

    events = (void *)rbh_hashmap_get(pool->pool, &event->id);
    if (events) {
        rc = deduplicate_event(pool, events, event);
        if (rc)
            return rc;

    } else {
        /* we do not insert NULL values in the map */
        assert(errno == ENOENT);
        rc = rbh_fsevent_pool_insert_new_entry(pool, event);
        if (rc)
            return rc;

        errno = 0;
    }

    if (rbh_fsevent_pool_is_full(pool))
        /* Notify the caller that the pool is now full */
        errno = ENOSPC;

    return 1;
}

struct rbh_iterator *
rbh_fsevent_pool_flush(struct rbh_fsevent_pool *pool)
{
    struct rbh_fsevent_node *elem, *tmp;
    size_t count = 0;

    rbh_list_foreach_safe(&pool->events, elem, tmp, link)
        rbh_list_del(&elem->link);

    if (pool->count == 0)
        return NULL;

    while (pool->count > 0 && count < pool->flush_size) {
        struct rbh_list_node *first_events;
        struct rbh_id_node *first_id;

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

    return rbh_iter_list(&pool->events,
                         offsetof(struct rbh_fsevent_node, link));
}

